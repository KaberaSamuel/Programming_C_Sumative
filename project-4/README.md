# Sales Transaction Processing and Analysis

I use this program to enter sales, find transactions, and see how much each product earned.

## Demonstration

I use three sample sales to check the calculations, filters, and save and load options.

From this folder, build the program and make a copy of the sample data.

```bash
demo_dir=$(mktemp -d)
cp demo/transactions.txt "$demo_dir/transactions.txt"
gcc -std=c11 -Wall -Wextra -Wpedantic sales.c -o "$demo_dir/sales"
"$demo_dir/sales" "$demo_dir/transactions.txt"
```

1. Choose **5** and then **1** to calculate revenue. Choose **6**, then **2**, and enter `Pen` to see the two matching sales.
2. Choose **7** and then **1** to find transaction `202`. Choose **8** and then **3** to sort by total from low to high.
3. Choose **10** to save and **11** to reload. Choose **9** and enter `Pen` to show the sales report and the revenue for pens.

![VS Code terminal showing total and average sales, highest and lowest sales, and pen revenue](demo/terminal.png)

### Terminal output

```text
Processing option: Total revenue: 75.00
Matching transactions: 2

Sales report
Transactions: 3
Total revenue: 75.00
Average transaction: 25.00
Highest transaction: 203 | Pen | 3 units | Unit price 12.00 | Total 36.00 | 2026-11-20
Lowest transaction: 202 | Notebook | 1 units | Unit price 15.00 | Total 15.00 | 2026-11-19
Total units sold: 6
Revenue for Pen: 60.00
```

### Supporting files

- [Sample transactions](demo/transactions.txt)
- [Sales report output](demo/sales-report.txt)

## Build and run

From this folder:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sales.c -o sales
./sales
```

The program uses `transactions.txt` for saved data.

## How it works

- I keep transactions in a dynamic array. It grows with `realloc` when it fills up. Each transaction stores its price and total in cents.
- The processing menu uses an array of function pointers to call the selected calculation. The filter takes a callback that checks the amount or product. I use a comparison callback in selection sort to put the sales in order.
- I check for duplicate IDs, invalid prices, dates, and quantities. Product matching uses the exact name, including capital letters.
- I save one transaction per line in a text file. The program calculates the total again when it loads each row. It asks before replacing unsaved work and offers to save before exiting.
