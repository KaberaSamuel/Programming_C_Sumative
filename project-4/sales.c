#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEXT_SIZE 64
#define LINE_SIZE 512

typedef struct {
    int id;
    char product[TEXT_SIZE];
    int quantity;
    long long price;
    long long total;
    char date[11];
} Transaction;

typedef struct {
    Transaction *items;
    size_t count;
    size_t capacity;
} Sales;

static int read_line(FILE *file, char *line, size_t size)
{
    if (!fgets(line, size, file))
        return 0;
    char *newline = strchr(line, '\n');
    if (newline) {
        *newline = '\0';
    } else if (!feof(file)) {
        int ch;
        while ((ch = fgetc(file)) != '\n' && ch != EOF) {
        }
        return -1;
    }
    size_t length = strlen(line);
    if (length && line[length - 1] == '\r')
        line[length - 1] = '\0';
    return 1;
}

static int prompt(const char *label, char *line)
{
    printf("%s", label);
    fflush(stdout);
    return read_line(stdin, line, LINE_SIZE);
}

static int parse_number(const char *text, int min, int max, int *value)
{
    char *end;
    errno = 0;
    long number = strtol(text, &end, 10);
    if (errno || end == text || *end || number < min || number > max)
        return 0;
    *value = (int)number;
    return 1;
}

static int ask_number(const char *label, int min, int max, int *value)
{
    char line[LINE_SIZE];
    int status = prompt(label, line);
    if (status == 1 && parse_number(line, min, max, value))
        return 1;
    if (status != 0)
        printf("Enter a whole number from %d to %d.\n", min, max);
    return 0;
}

static int valid_text(const char *text)
{
    size_t length = strlen(text);
    if (length == 0 || length >= TEXT_SIZE || strchr(text, '|'))
        return 0;
    int has_text = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (iscntrl(ch))
            return 0;
        if (!isspace(ch))
            has_text = 1;
    }
    return has_text;
}

static int ask_text(const char *label, char *value)
{
    char line[LINE_SIZE];
    int status = prompt(label, line);
    if (status == 1 && valid_text(line)) {
        strcpy(value, line);
        return 1;
    }
    if (status != 0)
        puts("Enter 1 to 63 characters without a vertical bar or control characters.");
    return 0;
}

/* Read money as cents, keeping the two decimal digits exact. */
static int parse_money(const char *text, long long *cents)
{
    if (!isdigit((unsigned char)text[0]))
        return 0;
    char *end;
    errno = 0;
    long long whole = strtoll(text, &end, 10);
    if (errno || whole > (LLONG_MAX - 99) / 100)
        return 0;
    int fraction = 0;
    if (*end == '.') {
        end++;
        if (!isdigit((unsigned char)*end))
            return 0;
        fraction = 10 * (*end++ - '0');
        if (isdigit((unsigned char)*end))
            fraction += *end++ - '0';
    }
    if (*end)
        return 0;
    *cents = whole * 100 + fraction;
    return 1;
}

static int ask_money(const char *label, long long *value)
{
    char line[LINE_SIZE];
    int status = prompt(label, line);
    if (status == 1 && parse_money(line, value))
        return 1;
    if (status != 0)
        puts("Enter a nonnegative amount with up to two decimal places.");
    return 0;
}

static void show_money(long long cents)
{
    printf("%lld.%02lld", cents / 100, cents % 100);
}

static int valid_date(const char *date)
{
    if (strlen(date) != 10 || date[4] != '-' || date[7] != '-')
        return 0;
    for (int i = 0; i < 10; i++) {
        if (i != 4 && i != 7 && !isdigit((unsigned char)date[i]))
            return 0;
    }
    int year, month, day;
    if (sscanf(date, "%4d-%2d-%2d", &year, &month, &day) != 3 ||
        year < 1 || month < 1 || month > 12 || day < 1)
        return 0;
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
        days[1] = 29;
    return day <= days[month - 1];
}

static Transaction *find_id(Sales *sales, int id)
{
    for (size_t i = 0; i < sales->count; i++) {
        if (sales->items[i].id == id)
            return &sales->items[i];
    }
    return NULL;
}

static Transaction *select_transaction(Sales *sales)
{
    int id;
    if (!ask_number("Transaction ID: ", 1, INT_MAX, &id))
        return NULL;
    Transaction *item = find_id(sales, id);
    if (!item)
        puts("Transaction not found.");
    return item;
}

static int append_transaction(Sales *sales, Transaction item)
{
    if (sales->count == sales->capacity) {
        size_t capacity = sales->capacity ? sales->capacity * 2 : 4;
        Transaction *items;
        if (sales->items)
            items = realloc(sales->items, capacity * sizeof *items);
        else
            items = malloc(capacity * sizeof *items);
        if (!items) {
            puts("Could not allocate memory for the transaction.");
            return 0;
        }
        sales->items = items;
        sales->capacity = capacity;
    }
    sales->items[sales->count++] = item;
    return 1;
}

static int set_total(Transaction *item)
{
    if (item->price > LLONG_MAX / item->quantity)
        return 0;
    item->total = item->price * item->quantity;
    return 1;
}

static int read_transaction(Transaction *item)
{
    char date[LINE_SIZE];
    if (!ask_text("Product: ", item->product) ||
        !ask_number("Quantity: ", 1, INT_MAX, &item->quantity) ||
        !ask_money("Unit price: ", &item->price))
        return 0;
    if (!set_total(item)) {
        puts("The transaction total is too large.");
        return 0;
    }
    if (prompt("Date (YYYY-MM-DD): ", date) != 1 || !valid_date(date)) {
        puts("Enter a valid date as YYYY-MM-DD.");
        return 0;
    }
    strcpy(item->date, date);
    return 1;
}

static int add_transaction(Sales *sales)
{
    Transaction item = {0};
    if (!ask_number("New transaction ID: ", 1, INT_MAX, &item.id))
        return 0;
    if (find_id(sales, item.id)) {
        puts("That transaction ID already exists.");
        return 0;
    }
    return read_transaction(&item) && append_transaction(sales, item);
}

static void show_transaction(const Transaction *item)
{
    printf("%d | %s | %d units | Unit price ", item->id, item->product, item->quantity);
    show_money(item->price);
    printf(" | Total ");
    show_money(item->total);
    printf(" | %s\n", item->date);
}

static void list_transactions(const Sales *sales)
{
    if (!sales->count)
        puts("No transactions recorded.");
    for (size_t i = 0; i < sales->count; i++)
        show_transaction(&sales->items[i]);
}

static int update_transaction(Sales *sales)
{
    Transaction *item = select_transaction(sales);
    if (!item)
        return 0;
    Transaction updated = *item;
    if (!read_transaction(&updated))
        return 0;
    *item = updated;
    return 1;
}

static int confirm(const char *label)
{
    char answer[LINE_SIZE];
    return prompt(label, answer) == 1 &&
           (strcmp(answer, "y") == 0 || strcmp(answer, "Y") == 0);
}

static int delete_transaction(Sales *sales)
{
    Transaction *item = select_transaction(sales);
    if (!item || !confirm("Delete this transaction? [y/N] "))
        return 0;
    size_t index = (size_t)(item - sales->items);
    sales->count--;
    for (size_t i = index; i < sales->count; i++)
        sales->items[i] = sales->items[i + 1];
    return 1;
}

static long double revenue(const Sales *sales, const char *product)
{
    long double cents = 0;
    for (size_t i = 0; i < sales->count; i++) {
        const Transaction *item = &sales->items[i];
        if (!product || strcmp(item->product, product) == 0)
            cents += item->total;
    }
    return cents;
}

static void show_revenue(const Sales *sales)
{
    printf("Total revenue: %.2Lf\n", revenue(sales, NULL) / 100);
}

static void show_average(const Sales *sales)
{
    if (sales->count)
        printf("Average transaction: %.2Lf\n", revenue(sales, NULL) / sales->count / 100);
    else
        puts("Average transaction: no sales yet.");
}

static const Transaction *extreme(const Sales *sales, int highest)
{
    if (!sales->count)
        return NULL;
    const Transaction *best = &sales->items[0];
    for (size_t i = 1; i < sales->count; i++) {
        const Transaction *item = &sales->items[i];
        if ((highest && item->total > best->total) || (!highest && item->total < best->total))
            best = item;
    }
    return best;
}

static void show_highest(const Sales *sales)
{
    const Transaction *item = extreme(sales, 1);
    printf("Highest transaction: ");
    if (item)
        show_transaction(item);
    else
        puts("no sales yet.");
}

static void show_lowest(const Sales *sales)
{
    const Transaction *item = extreme(sales, 0);
    printf("Lowest transaction: ");
    if (item)
        show_transaction(item);
    else
        puts("no sales yet.");
}

static void show_units(const Sales *sales)
{
    unsigned long long units = 0;
    for (size_t i = 0; i < sales->count; i++)
        units += sales->items[i].quantity;
    printf("Total units sold: %llu\n", units);
}

static void process_sales(const Sales *sales)
{
    void (*operations[])(const Sales *) = {
        show_revenue, show_average, show_highest, show_lowest, show_units
    };
    int choice;
    puts("1. Revenue  2. Average  3. Highest  4. Lowest  5. Units sold");
    if (ask_number("Processing option: ", 1, 5, &choice))
        operations[choice - 1](sales);
}

typedef int (*Match)(const Transaction *, const char *, long long);

static size_t show_matches(const Sales *sales, Match match,
                           const char *product, long long amount)
{
    size_t count = 0;
    for (size_t i = 0; i < sales->count; i++) {
        const Transaction *item = &sales->items[i];
        if (match(item, product, amount)) {
            show_transaction(item);
            count++;
        }
    }
    return count;
}

static int above_amount(const Transaction *item, const char *product, long long amount)
{
    (void)product;
    return item->total > amount;
}

static int exact_amount(const Transaction *item, const char *product, long long amount)
{
    (void)product;
    return item->total == amount;
}

static int same_product(const Transaction *item, const char *product, long long amount)
{
    (void)amount;
    return strcmp(item->product, product) == 0;
}

static void filter_transactions(const Sales *sales)
{
    int choice;
    size_t count;
    if (!ask_number("1. Above an amount  2. By product: ", 1, 2, &choice))
        return;
    if (choice == 1) {
        long long amount;
        if (!ask_money("Amount: ", &amount))
            return;
        count = show_matches(sales, above_amount, NULL, amount);
    } else {
        char product[TEXT_SIZE];
        if (!ask_text("Product: ", product))
            return;
        count = show_matches(sales, same_product, product, 0);
    }
    printf("Matching transactions: %zu\n", count);
}

static void search_transactions(Sales *sales)
{
    int choice;
    size_t count;
    if (!ask_number("1. ID  2. Product  3. Exact amount: ", 1, 3, &choice))
        return;
    if (choice == 1) {
        Transaction *item = select_transaction(sales);
        if (item)
            show_transaction(item);
        return;
    }
    if (choice == 2) {
        char product[TEXT_SIZE];
        if (!ask_text("Product: ", product))
            return;
        count = show_matches(sales, same_product, product, 0);
    } else {
        long long amount;
        if (!ask_money("Amount: ", &amount))
            return;
        count = show_matches(sales, exact_amount, NULL, amount);
    }
    if (!count)
        puts("No matching transactions.");
}

typedef int (*ComesBefore)(const Transaction *, const Transaction *);

static int by_id(const Transaction *a, const Transaction *b)
{
    return a->id < b->id;
}

static int by_product(const Transaction *a, const Transaction *b)
{
    return strcmp(a->product, b->product) < 0;
}

static int by_small_amount(const Transaction *a, const Transaction *b)
{
    return a->total < b->total;
}

static int by_large_amount(const Transaction *a, const Transaction *b)
{
    return a->total > b->total;
}

static void selection_sort(Sales *sales, ComesBefore before)
{
    for (size_t i = 0; i < sales->count; i++) {
        size_t best = i;
        for (size_t j = i + 1; j < sales->count; j++) {
            if (before(&sales->items[j], &sales->items[best]))
                best = j;
        }
        Transaction saved = sales->items[i];
        sales->items[i] = sales->items[best];
        sales->items[best] = saved;
    }
}

static int sort_transactions(Sales *sales)
{
    ComesBefore orders[] = {by_id, by_product, by_small_amount, by_large_amount};
    int choice;
    if (!ask_number("1. ID  2. Product  3. Amount low to high  4. Amount high to low: ",
                    1, 4, &choice))
        return 0;
    selection_sort(sales, orders[choice - 1]);
    list_transactions(sales);
    return sales->count > 1;
}

static void sales_report(const Sales *sales)
{
    printf("\nSales report\nTransactions: %zu\n", sales->count);
    show_revenue(sales);
    show_average(sales);
    show_highest(sales);
    show_lowest(sales);
    show_units(sales);
    if (sales->count) {
        char product[TEXT_SIZE];
        if (ask_text("Product for revenue: ", product))
            printf("Revenue for %s: %.2Lf\n", product, revenue(sales, product) / 100);
    }
}

static int split_row(char *line, char **fields, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        fields[i] = line;
        char *bar = strchr(line, '|');
        if (i + 1 == count)
            return bar == NULL;
        if (!bar)
            return 0;
        *bar = '\0';
        line = bar + 1;
    }
    return 0;
}

static int load_transaction(char *line, Sales *sales)
{
    char *field[5];
    Transaction item = {0};
    if (!split_row(line, field, 5) || !parse_number(field[0], 1, INT_MAX, &item.id) ||
        find_id(sales, item.id) || !valid_text(field[1]) ||
        !parse_number(field[2], 1, INT_MAX, &item.quantity) ||
        !parse_money(field[3], &item.price) || !set_total(&item) || !valid_date(field[4]))
        return 0;
    strcpy(item.product, field[1]);
    strcpy(item.date, field[4]);
    return append_transaction(sales, item);
}

static int load_sales(const char *path, Sales *sales, int startup)
{
    FILE *file = fopen(path, "r");
    if (!file) {
        if (startup && errno == ENOENT)
            return 1;
        perror(path);
        return 0;
    }
    Sales loaded = {0};
    char line[LINE_SIZE];
    int status;
    while ((status = read_line(file, line, sizeof line)) == 1) {
        if (!load_transaction(line, &loaded)) {
            status = -1;
            break;
        }
    }
    int failed = status < 0 || ferror(file);
    if (fclose(file) != 0)
        failed = 1;
    if (failed) {
        fprintf(stderr, "Could not load sales from %s. Check the file and its format.\n", path);
        free(loaded.items);
        return 0;
    }
    free(sales->items);
    *sales = loaded;
    return 1;
}

static int write_sales(FILE *file, const Sales *sales)
{
    for (size_t i = 0; i < sales->count; i++) {
        const Transaction *item = &sales->items[i];
        if (fprintf(file, "%d|%s|%d|%lld.%02lld|%s\n", item->id, item->product, item->quantity,
                    item->price / 100, item->price % 100, item->date) < 0)
            return 0;
    }
    return 1;
}

static int save_sales(const char *path, const Sales *sales)
{
    const char suffix[] = ".tmp.XXXXXX";
    char *temporary = malloc(strlen(path) + sizeof suffix);
    if (!temporary) {
        fputs("Could not allocate memory to save sales.\n", stderr);
        return 0;
    }
    sprintf(temporary, "%s%s", path, suffix);
    int fd = mkstemp(temporary);
    if (fd < 0) {
        perror(path);
        free(temporary);
        return 0;
    }
    FILE *file = fdopen(fd, "w");
    int saved = 0;
    if (file) {
        int written = write_sales(file, sales);
        int closed = fclose(file) == 0;
        saved = written && closed && rename(temporary, path) == 0;
    } else {
        close(fd);
    }
    if (saved) {
        puts("Transactions saved.");
    } else {
        fprintf(stderr, "Could not save sales to %s.\n", path);
        remove(temporary);
    }
    free(temporary);
    return saved;
}

static void show_menu(void)
{
    puts("\nSales Transaction Management\n"
         "1. Add a transaction\n2. List transactions\n3. Update a transaction\n"
         "4. Delete a transaction\n5. Process sales data\n6. Filter transactions\n"
         "7. Search transactions\n8. Sort transactions\n9. Generate a sales report\n"
         "10. Save transactions\n11. Load transactions\n0. Exit");
}

int main(int argc, char **argv)
{
    if (argc > 2) {
        fputs("Usage: ./sales [data-file]\n", stderr);
        return 1;
    }
    const char *path = argc == 2 ? argv[1] : "transactions.txt";
    Sales sales = {0};
    if (!load_sales(path, &sales, 1))
        return 1;
    printf("Loaded %zu transactions.\n", sales.count);
    int unsaved = 0;
    while (!feof(stdin) && !ferror(stdin)) {
        int choice, changed = 0;
        show_menu();
        if (!ask_number("Choose an option: ", 0, 11, &choice))
            continue;
        if (choice == 0) {
            if (!unsaved || !confirm("Save changes before exiting? [y/N] ") ||
                save_sales(path, &sales)) {
                unsaved = 0;
                break;
            }
            continue;
        }
        switch (choice) {
        case 1: changed = add_transaction(&sales); break;
        case 2: list_transactions(&sales); break;
        case 3: changed = update_transaction(&sales); break;
        case 4: changed = delete_transaction(&sales); break;
        case 5: process_sales(&sales); break;
        case 6: filter_transactions(&sales); break;
        case 7: search_transactions(&sales); break;
        case 8:
            if (sort_transactions(&sales))
                unsaved = 1;
            break;
        case 9: sales_report(&sales); break;
        case 10:
            if (save_sales(path, &sales))
                unsaved = 0;
            break;
        case 11:
            if ((!unsaved || confirm("Replace unsaved changes? [y/N] ")) &&
                load_sales(path, &sales, 0)) {
                unsaved = 0;
                printf("Loaded %zu transactions.\n", sales.count);
            }
            break;
        }
        if (changed) {
            puts("Transaction updated.");
            unsaved = 1;
        }
    }
    free(sales.items);
    if (unsaved)
        fputs("Ended with unsaved changes.\n", stderr);
    return unsaved ? 1 : 0;
}
