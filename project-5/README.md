# Multithreaded Document Search

I use this program to search several text files for the same keyword. It saves a report for each file.

## Build and run

From this folder:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -pthread document_analyzer.c -o document_analyzer
```

Run `./document_analyzer` with the keyword first and the paths of the text files after it. Put quotes around a keyword or a path that contains spaces. The reports are saved in the current directory. If a name already exists, the program adds a number.

## How it works

- I give each file its own thread. Each thread reads its file and writes its report. The main thread starts the workers, waits for them with `pthread_join`, and prints the results in input order.
- I read one line at a time. I count matches anywhere in the line and ignore letter case for the English alphabet. After a match, I continue after the matched text. A line with several hits appears once in the list of matching line numbers.
- A file with zero matches still gets a successful report. If an input is missing or cannot be read, the worker writes a failed report when it can. The program returns a nonzero exit status if any file fails.
