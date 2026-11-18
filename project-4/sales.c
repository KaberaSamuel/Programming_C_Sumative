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

static void show_menu(void)
{
    puts("\nSales Transaction Management\n"
         "1. Add a transaction\n2. List transactions\n3. Update a transaction\n"
         "4. Delete a transaction\n0. Exit");
}

int main(void)
{
    Sales sales = {0};
    while (!feof(stdin) && !ferror(stdin)) {
        int choice, changed = 0;
        show_menu();
        if (!ask_number("Choose an option: ", 0, 4, &choice))
            continue;
        if (choice == 0)
            break;
        switch (choice) {
        case 1: changed = add_transaction(&sales); break;
        case 2: list_transactions(&sales); break;
        case 3: changed = update_transaction(&sales); break;
        case 4: changed = delete_transaction(&sales); break;
        }
        if (changed)
            puts("Transaction updated.");
    }
    free(sales.items);
    return 0;
}
