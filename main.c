#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <direct.h>
    #define PATH_SEP "\\"
    #define mkdir_cross(path) _mkdir(path)
#else
    #include <unistd.h>
    #define PATH_SEP "/"
    #define mkdir_cross(path) mkdir(path, 0755)
#endif

#define MAX_MENU 50
#define MAX_ORDER 100
#define MAX_NAME 100
#define MAX_FILENAME 260
#define COPY_BUF_SIZE 8192

typedef struct {
    char name[MAX_NAME];
    int price;
} MenuItem;

typedef struct {
    char username[MAX_NAME];
    char social_number[40];
    int isMember;
    char filename[MAX_FILENAME];
} Customer;

typedef struct {
    int menuIndex;
    int spiceLevel;
    int orderNumber;
} OrderItem;

typedef struct {
    char name[MAX_NAME];
    int count;
    float revenue;
} ItemStats;

int fileExists(const char *filename);
int copy_file_binary(const char *src, const char *dst);
void loadMenu(MenuItem menu[], int *menuCount);
void showMenu(MenuItem menu[], int menuCount);
void showCurrentOrder(MenuItem menu[], OrderItem order[], int orderCount);
void takeOrder(MenuItem menu[], int menuCount, OrderItem order[], int *orderCount);
float calculateTotal(MenuItem menu[], OrderItem order[], int orderCount);
void processImages(MenuItem menu[], OrderItem order[], int orderCount);
void printReceipt(Customer c, MenuItem menu[], OrderItem order[], int orderCount, float total, float discount, float membershipFee);
void saveCustomerData(Customer c, MenuItem menu[], OrderItem order[], int orderCount, float total);
void createCustomerFile(Customer *c);
void resetServingFolder();
const char* getSpiceName(int level);
void saveStatistics(Customer c, MenuItem menu[], OrderItem order[], int orderCount, float total, float discount, float membershipFee);
void createDirectoryRecursive(const char *path);

int main(void) {
    Customer c;
    MenuItem menu[MAX_MENU];
    int menuCount = 0;
    OrderItem order[MAX_ORDER];
    int orderCount = 0;
    float total;
    float subtotal;
    float totalDiscount = 0.0f;
    float membershipFee = 0.0f;

    resetServingFolder();

    printf("==============================================\n");
    printf("=   Welcome to The Spicy Spoon Restaurant    =\n");
    printf("==============================================\n\n");

    printf("Enter username: ");
    if (scanf("%99s", c.username) != 1) {
        printf("Invalid input\n");
        return 1;
    }
    
    printf("Enter social number: ");
    if (scanf("%39s", c.social_number) != 1) {
        printf("Invalid input\n");
        return 1;
    }

    snprintf(c.filename, sizeof(c.filename), "customer_data%s%s_%s.txt", 
             PATH_SEP, c.username, c.social_number);

    if (fileExists(c.filename)) {
        FILE *f = fopen(c.filename, "r");
        if (f) {
            c.isMember = 0;
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (sscanf(line, "Member: %d", &c.isMember) == 1) break;
            }
            fclose(f);
        } else {
            c.isMember = 0;
        }
        printf("\nWelcome back, %s! (Member: %s)\n", 
               c.username, c.isMember ? "Yes" : "No");
    } else {
        printf("\nNew customer detected. Creating profile...\n");
        c.isMember = 0;
        createCustomerFile(&c);
    }

    loadMenu(menu, &menuCount);
    if (menuCount == 0) {
        printf("\nError: No menu found!\n");
        printf("Please create: menu_data%smenu.txt\n", PATH_SEP);
        printf("Format: <ItemName> <price>\n");
        printf("Example:\n");
        printf("  PadThai 120\n");
        printf("  TomYum 150\n");
        printf("  GreenCurry 180\n");
        return 1;
    }

    showMenu(menu, menuCount);
    takeOrder(menu, menuCount, order, &orderCount);

    if (orderCount == 0) {
        printf("\nNo items ordered. Thank you for visiting!\n");
        return 0;
    }

    processImages(menu, order, orderCount);

    subtotal = calculateTotal(menu, order, orderCount);
    total = subtotal;
    
    printf("\n==============================================\n");
    printf("              PAYMENT SUMMARY\n");
    printf("==============================================\n");
    printf(" Subtotal:                    %7.2f baht \n", subtotal);
    
    if (subtotal >= 1000.0f) {
        float largeDiscount = subtotal * 0.05f;
        totalDiscount += largeDiscount;
        total -= largeDiscount;
        printf(" Large order discount (5%%):   -%7.2f baht \n", largeDiscount);
    }

    if (!c.isMember) {
        char ch;
        printf("==============================================\n");
        printf("You are not a member yet.\n");
        printf("Register for only 20 baht? (y/n): ");
        scanf(" %c", &ch);
        if (ch == 'y' || ch == 'Y') {
            membershipFee = 20.0f;
            total += membershipFee;
            c.isMember = 1;
            
            // Read existing file content
            char *fileContent = NULL;
            long fileSize = 0;
            FILE *f = fopen(c.filename, "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                fileSize = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                fileContent = (char*)malloc(fileSize + 1);
                if (fileContent) {
                    fread(fileContent, 1, fileSize, f);
                    fileContent[fileSize] = '\0';
                }
                fclose(f);
            }
            
            // Rewrite file with updated membership status
            f = fopen(c.filename, "w");
            if (f) {
                fprintf(f, "Username: %s\nSocial: %s\nMember: %d\n", 
                        c.username, c.social_number, c.isMember);
                
                // Append old order history if it exists
                if (fileContent) {
                    char *orderStart = strstr(fileContent, "\n--- New Order ---");
                    if (orderStart) {
                        fprintf(f, "%s", orderStart);
                    }
                    free(fileContent);
                }
                fclose(f);
            }
            
            printf("Registered as member!\n");
            printf(" Membership fee:              +%7.2f baht \n", membershipFee);
        }
    }

    if (c.isMember) {
        float memberDiscount = total * 0.10f;
        totalDiscount += memberDiscount;
        total -= memberDiscount;
        printf(" Member discount (10%%):       -%7.2f baht \n", memberDiscount);
    }
    
    printf("==============================================\n");
    if (totalDiscount > 0.0f) {
        printf(" Total discount saved:        -%7.2f baht \n", totalDiscount);
    }
    printf(" FINAL TOTAL:                  %7.2f baht \n", total);
    printf("==============================================\n");

    printReceipt(c, menu, order, orderCount, total, totalDiscount, membershipFee);
    saveCustomerData(c, menu, order, orderCount, total);
    saveStatistics(c, menu, order, orderCount, total, totalDiscount, membershipFee);

    printf("\nAll serving images saved to: serving_folder%s\n", PATH_SEP);
    printf("Each item gets its own folder with images\n");
    printf("\nThank you for dining with us, %s!\n", c.username);
    
    return 0;
}

int fileExists(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f) { 
        fclose(f); 
        return 1; 
    }
    return 0;
}

int copy_file_binary(const char *src, const char *dst) {
    FILE *inf = fopen(src, "rb");
    if (!inf) return 0;
    
    FILE *outf = fopen(dst, "wb");
    if (!outf) { 
        fclose(inf); 
        return 0; 
    }

    char *buf = (char*)malloc(COPY_BUF_SIZE);
    if (!buf) { 
        fclose(inf); 
        fclose(outf); 
        return 0; 
    }

    size_t n;
    while ((n = fread(buf, 1, COPY_BUF_SIZE, inf)) > 0) {
        if (fwrite(buf, 1, n, outf) != n) {
            free(buf);
            fclose(inf);
            fclose(outf);
            return 0;
        }
    }
    
    free(buf);
    fclose(inf);
    fclose(outf);
    return 1;
}

void loadMenu(MenuItem menu[], int *menuCount) {
    *menuCount = 0;
    char filename[MAX_FILENAME];
    snprintf(filename, sizeof(filename), "menu_data%smenu.txt", PATH_SEP);
    
    FILE *f = fopen(filename, "r");
    if (!f) return;
    
    while (*menuCount < MAX_MENU) {
        char name[MAX_NAME];
        int price;
        if (fscanf(f, "%99s %d", name, &price) != 2) break;
        
        strncpy(menu[*menuCount].name, name, MAX_NAME-1);
        menu[*menuCount].name[MAX_NAME-1] = '\0';
        menu[*menuCount].price = price;
        (*menuCount)++;
    }
    fclose(f);
}

void showMenu(MenuItem menu[], int menuCount) {
    printf("\n================= MENU =================\n");
    for (int i = 0; i < menuCount; ++i) {
        printf(" %2d. %-25s %4d baht \n", 
               i + 1, menu[i].name, menu[i].price);
    }
    printf("========================================\n\n");
}

const char* getSpiceName(int level) {
    switch(level) {
        case 0: return "No Spice";
        case 1: return "Mild";
        case 2: return "Medium";
        case 3: return "Hot";
        default: return "Unknown";
    }
}

void showCurrentOrder(MenuItem menu[], OrderItem order[], int orderCount) {
    if (orderCount == 0) {
        printf("\nYour cart is empty\n");
        return;
    }
    
    printf("\n================ YOUR CART ================\n");
    printf("  #  Item                    Spice   Price \n");
    printf("===========================================\n");
    
    float subtotal = 0.0f;
    for (int i = 0; i < orderCount; ++i) {
        int idx = order[i].menuIndex;
        printf("  %-2d %-23s %-7s %4d baht \n", 
               order[i].orderNumber,
               menu[idx].name,
               getSpiceName(order[i].spiceLevel),
               menu[idx].price);
        subtotal += menu[idx].price;
    }
    
    printf("===========================================\n");
    printf("  Subtotal:                        %6.0f baht \n", subtotal);
    printf("===========================================\n");
}

void takeOrder(MenuItem menu[], int menuCount, OrderItem order[], int *orderCount) {
    *orderCount = 0;
    
    printf("Start ordering!\n");
    printf("-------------------------------------------\n");
    printf("Tips:\n");
    printf("   - Enter menu number (1-%d) to add item\n", menuCount);
    printf("   - Enter negative number (-1, -2, etc.) to cancel item\n");
    printf("   - Enter 0 to finish and confirm order\n");
    printf("   - You CAN order same item multiple times!\n");
    printf("-------------------------------------------\n");
    
    while (1) {
        int choice;
        
        printf("\nMenu number (-%d to %d) or 0 to finish: ", *orderCount, menuCount);
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        
        if (choice == 0) {
            if (*orderCount > 0) {
                char confirm;
                for (int i = 0; i < *orderCount; ++i) {
                    order[i].orderNumber = i + 1;
                }
                showCurrentOrder(menu, order, *orderCount);
                printf("\nConfirm this order? (y/n): ");
                scanf(" %c", &confirm);
                if (confirm == 'y' || confirm == 'Y') {
                    printf("Order confirmed!\n");
                    break;
                } else {
                    printf("Continue ordering...\n");
                    continue;
                }
            } else {
                printf("Cart is empty! Add items first.\n");
                continue;
            }
        }
        
        if (choice < 0) {
            int cancelNum = -choice;
            
            if (cancelNum < 1 || cancelNum > *orderCount) {
                printf("Item #%d not in cart (valid: 1-%d)\n", cancelNum, *orderCount);
                showCurrentOrder(menu, order, *orderCount);
                continue;
            }
            
            int cancelIndex = cancelNum - 1;
            
            printf("Cancelled: #%d %s (%s)\n", 
                   order[cancelIndex].orderNumber,
                   menu[order[cancelIndex].menuIndex].name,
                   getSpiceName(order[cancelIndex].spiceLevel));
            
            for (int j = cancelIndex; j < *orderCount - 1; ++j) {
                order[j] = order[j + 1];
            }
            (*orderCount)--;
            
            for (int i = 0; i < *orderCount; ++i) {
                order[i].orderNumber = i + 1;
            }
            
            showCurrentOrder(menu, order, *orderCount);
            continue;
        }
        
        if (choice < 1 || choice > menuCount) {
            printf("Invalid choice. Choose between 1-%d\n", menuCount);
            continue;
        }
        
        if (*orderCount >= MAX_ORDER) {
            printf("Maximum order limit reached!\n");
            break;
        }
        
        int idx = choice - 1;
        
        int spice;
        printf("  Spice level for %s:\n", menu[idx].name);
        printf("   0 = No Spice   1 = Mild   2 = Medium   3 = Hot\n");
        printf("   Enter level (0-3): ");
        
        if (scanf("%d", &spice) != 1) {
            spice = 0;
            while (getchar() != '\n');
        }
        
        if (spice < 0) spice = 0;
        if (spice > 3) spice = 3;
        
        order[*orderCount].menuIndex = idx;
        order[*orderCount].spiceLevel = spice;
        order[*orderCount].orderNumber = *orderCount + 1;
        
        printf("Added #%d: %s (%s)\n", 
               *orderCount + 1, menu[idx].name, getSpiceName(spice));
        
        (*orderCount)++;
        
        showCurrentOrder(menu, order, *orderCount);
    }
}

float calculateTotal(MenuItem menu[], OrderItem order[], int orderCount) {
    float total = 0.0f;
    for (int i = 0; i < orderCount; ++i) {
        total += (float)menu[order[i].menuIndex].price;
    }
    return total;
}

void createDirectoryRecursive(const char *path) {
    char tmp[MAX_FILENAME];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\')
        tmp[len - 1] = 0;
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = 0;
            mkdir_cross(tmp);
            *p = PATH_SEP[0];
        }
    }
    mkdir_cross(tmp);
}

void processImages(MenuItem menu[], OrderItem order[], int orderCount) {
    char src[MAX_FILENAME], dst[MAX_FILENAME], folderPath[MAX_FILENAME];
    
    if (orderCount == 0) {
        printf("\nNo items ordered - no images to copy.\n");
        return;
    }

    printf("\nProcessing images...\n");
    printf("-------------------------------------------\n");
    
    for (int i = 0; i < orderCount; ++i) {
        const char *itemName = menu[order[i].menuIndex].name;
        int orderNum = order[i].orderNumber;
        int spice = order[i].spiceLevel;

        snprintf(folderPath, sizeof(folderPath), "serving_folder%s%s_order%d", 
                PATH_SEP, itemName, orderNum);
        
        createDirectoryRecursive(folderPath);

        snprintf(src, sizeof(src), "menu_picture%s%s.jpg", PATH_SEP, itemName);
        snprintf(dst, sizeof(dst), "%s%s%s.jpg", folderPath, PATH_SEP, itemName);

        if (fileExists(src)) {
            if (copy_file_binary(src, dst)) {
                printf("#%d: %s.jpg -> %s%s\n", orderNum, itemName, folderPath, PATH_SEP);
            } else {
                printf("Failed to copy: %s.jpg\n", itemName);
            }
        } else {
            printf("Image not found: %s.jpg (skipping)\n", itemName);
        }

        if (spice > 0) {
            snprintf(src, sizeof(src), "menu_picture%schili_%d.png", PATH_SEP, spice);
            snprintf(dst, sizeof(dst), "%s%schili_%d.png", folderPath, PATH_SEP, spice);
            
            if (fileExists(src)) {
                if (copy_file_binary(src, dst)) {
                    printf("#%d: Added %s indicator to folder\n", 
                           orderNum, getSpiceName(spice));
                } else {
                    printf("Failed to copy chili_%d.png\n", spice);
                }
            } else {
                printf("Chili image not found: chili_%d.png\n", spice);
            }
        } else {
            printf("#%d: No spice (original flavor)\n", orderNum);
        }
    }
}

void printReceipt(Customer c, MenuItem menu[], OrderItem order[], 
                 int orderCount, float total, float discount, float membershipFee) {
    printf("\n==============================================\n");
    printf("                   RECEIPT                    \n");
    printf("==============================================\n");
    printf(" Customer: %-31s \n", c.username);
    printf(" Member: %-33s \n", c.isMember ? "Yes" : "No");
    printf("==============================================\n");
    
    for (int i = 0; i < orderCount; ++i) {
        int idx = order[i].menuIndex;
        printf(" #%-2d %-23s %4d baht \n", 
               order[i].orderNumber,
               menu[idx].name,
               menu[idx].price);
        printf("     Spice: %-29s \n", getSpiceName(order[i].spiceLevel));
    }
    
    printf("==============================================\n");
    if (membershipFee > 0.0f) {
        printf(" Membership fee: +%24.2f baht \n", membershipFee);
    }
    if (discount > 0.0f) {
        printf(" Total discount: -%24.2f baht \n", discount);
    }
    printf(" FINAL TOTAL: %27.2f baht \n", total);
    printf("==============================================\n");
}

void saveCustomerData(Customer c, MenuItem menu[], OrderItem order[], 
                     int orderCount, float total) {
    FILE *f = fopen(c.filename, "a");
    if (!f) {
        printf("Error: Cannot save to %s\n", c.filename);
        return;
    }
    
    fprintf(f, "\n--- New Order ---\n");
    for (int i = 0; i < orderCount; ++i) {
        int idx = order[i].menuIndex;
        fprintf(f, "#%d: %s - %d baht (Spice: %d - %s)\n", 
                order[i].orderNumber,
                menu[idx].name, 
                menu[idx].price, 
                order[i].spiceLevel,
                getSpiceName(order[i].spiceLevel));
    }
    fprintf(f, "Total: %.2f baht\n", total);
    fprintf(f, "Member: %d\n", c.isMember);
    fclose(f);
}

void createCustomerFile(Customer *c) {
    createDirectoryRecursive("customer_data");
    
    FILE *f = fopen(c->filename, "w");
    if (!f) {
        printf("Error: Cannot create %s\n", c->filename);
        return;
    }
    fprintf(f, "Username: %s\nSocial: %s\nMember: %d\n", 
            c->username, c->social_number, c->isMember);
    fclose(f);
}

void resetServingFolder() {
    printf("Resetting serving folder...\n");
    
#ifdef _WIN32
    system("rd /S /Q serving_folder >nul 2>&1");
    system("mkdir serving_folder >nul 2>&1");
#else
    system("rm -rf serving_folder 2>/dev/null");
    system("mkdir -p serving_folder 2>/dev/null");
#endif
    
    printf("Serving folder ready.\n\n");
}

void saveStatistics(Customer c, MenuItem menu[], OrderItem order[], 
                   int orderCount, float total, float discount, float membershipFee) {
    FILE *f = fopen("spice_data.txt", "r");
    
    int totalTransactions = 0;
    float totalRevenue = 0.0f;
    float totalDiscounts = 0.0f;
    int totalItemsSold = 0;
    int memberCount = 0;
    int spiceCount[4] = {0};
    
    ItemStats items[MAX_MENU] = {0};
    int uniqueItems = 0;
    
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            sscanf(line, "Total Transactions: %d", &totalTransactions);
            sscanf(line, "Total Revenue: %f", &totalRevenue);
            sscanf(line, "Total Discounts: %f", &totalDiscounts);
            sscanf(line, "Total Items Sold: %d", &totalItemsSold);
            sscanf(line, "Member Purchases: %d", &memberCount);
            
            int spiceVal;
            if (sscanf(line, "  No Spice: %d", &spiceVal) == 1) spiceCount[0] = spiceVal;
            else if (sscanf(line, "  Mild: %d", &spiceVal) == 1) spiceCount[1] = spiceVal;
            else if (sscanf(line, "  Medium: %d", &spiceVal) == 1) spiceCount[2] = spiceVal;
            else if (sscanf(line, "  Hot: %d", &spiceVal) == 1) spiceCount[3] = spiceVal;
            
            char itemName[MAX_NAME];
            int itemCount;
            float itemRevenue;
            if (sscanf(line, "  %[^:]: %d orders, %f baht", itemName, &itemCount, &itemRevenue) == 3) {
                strncpy(items[uniqueItems].name, itemName, MAX_NAME-1);
                items[uniqueItems].name[MAX_NAME-1] = '\0';
                items[uniqueItems].count = itemCount;
                items[uniqueItems].revenue = itemRevenue;
                uniqueItems++;
            }
        }
        fclose(f);
    }
    
    totalTransactions++;
    totalRevenue += total;
    totalDiscounts += discount;
    totalItemsSold += orderCount;
    if (c.isMember) memberCount++;
    
    for (int i = 0; i < orderCount; i++) {
        int idx = order[i].menuIndex;
        int spice = order[i].spiceLevel;
        
        if (spice >= 0 && spice <= 3) {
            spiceCount[spice]++;
        }
        
        int found = 0;
        for (int j = 0; j < uniqueItems; j++) {
            if (strcmp(items[j].name, menu[idx].name) == 0) {
                items[j].count++;
                items[j].revenue += menu[idx].price;
                found = 1;
                break;
            }
        }
        
        if (!found && uniqueItems < MAX_MENU) {
            strncpy(items[uniqueItems].name, menu[idx].name, MAX_NAME-1);
            items[uniqueItems].name[MAX_NAME-1] = '\0';
            items[uniqueItems].count = 1;
            items[uniqueItems].revenue = (float)menu[idx].price;
            uniqueItems++;
        }
    }
    
    for (int i = 0; i < uniqueItems - 1; i++) {
        for (int j = 0; j < uniqueItems - i - 1; j++) {
            if (items[j].count < items[j + 1].count) {
                ItemStats temp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = temp;
            }
        }
    }
    
    f = fopen("spice_data.txt", "w");
    if (!f) {
        printf("Warning: Could not save statistics\n");
        return;
    }
    
    fprintf(f, "==============================================\n");
    fprintf(f, "     THE SPICY SPOON - STATISTICS REPORT     \n");
    fprintf(f, "==============================================\n\n");
    
    fprintf(f, "OVERALL STATISTICS:\n");
    fprintf(f, "-------------------------------------------\n");
    fprintf(f, "Total Transactions: %d\n", totalTransactions);
    fprintf(f, "Total Revenue: %.2f baht\n", totalRevenue);
    fprintf(f, "Total Discounts: %.2f baht\n", totalDiscounts);
    fprintf(f, "Total Items Sold: %d\n", totalItemsSold);
    fprintf(f, "Member Purchases: %d\n", memberCount);
    fprintf(f, "Average Order Value: %.2f baht\n", 
            totalTransactions > 0 ? totalRevenue / totalTransactions : 0.0f);

    fprintf(f, "\nSPICE LEVEL PREFERENCES:\n");
    fprintf(f, "-------------------------------------------\n");
    fprintf(f, "  No Spice: %d orders (%.1f%%)\n", 
            spiceCount[0], totalItemsSold > 0 ? spiceCount[0] * 100.0f / totalItemsSold : 0.0f);
    fprintf(f, "  Mild: %d orders (%.1f%%)\n", 
            spiceCount[1], totalItemsSold > 0 ? spiceCount[1] * 100.0f / totalItemsSold : 0.0f);
    fprintf(f, "  Medium: %d orders (%.1f%%)\n", 
            spiceCount[2], totalItemsSold > 0 ? spiceCount[2] * 100.0f / totalItemsSold : 0.0f);
    fprintf(f, "  Hot: %d orders (%.1f%%)\n", 
            spiceCount[3], totalItemsSold > 0 ? spiceCount[3] * 100.0f / totalItemsSold : 0.0f);
    
    fprintf(f, "\nTOP SELLING ITEMS:\n");
    fprintf(f, "-------------------------------------------\n");
    
    int displayCount = uniqueItems < 15 ? uniqueItems : 15;
    for (int i = 0; i < displayCount; i++) {
        fprintf(f, "  %s: %d orders (%.1f%%), %.2f baht revenue\n",
                items[i].name, items[i].count, 
                totalItemsSold > 0 ? items[i].count * 100.0f / totalItemsSold : 0.0f,
                items[i].revenue);
    }
    
    fprintf(f, "\n==============================================\n");
    fprintf(f, "Report generated after %d transactions\n", totalTransactions);
    fprintf(f, "==============================================\n");
    
    fclose(f);
}