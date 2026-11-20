# Multithreaded Document Search

I use this program to search several text files for the same keyword. It saves a report for each file.

## Demonstration

I search three files for the same keyword. One file has two hits on a single line, one has a lowercase hit, and one has no hits.

From this folder, build and run the program.

```bash
demo_dir=$(mktemp -d)
gcc -std=c11 -Wall -Wextra -Wpedantic -pthread document_analyzer.c -o "$demo_dir/document_analyzer"
"$demo_dir/document_analyzer" IoT demo/sensors.txt demo/network.txt demo/basics.txt
cat sensors_report.txt
```

The last command assumes a fresh run. If the report name already exists, open the numbered report shown in the terminal. The order of the worker messages can vary between runs.

![VS Code terminal showing three document workers, their keyword counts, and the final summary](demo/terminal.png)

### Terminal output

```text
Document Search
Keyword: IoT
Input files: 3

[Thread 1] Creating worker for demo/sensors.txt
[Thread 2] Creating worker for demo/network.txt
[Thread 3] Creating worker for demo/basics.txt
[Thread 2] Processing demo/network.txt
[Thread 1] Processing demo/sensors.txt
[Thread 3] Processing demo/basics.txt
[Thread 2] Saved network_report.txt
[Thread 2] Finished demo/network.txt: 1 occurrences on 1 of 2 lines
[Thread 1] Saved sensors_report.txt
[Thread 1] Finished demo/sensors.txt: 2 occurrences on 1 of 2 lines
[Thread 3] Saved basics_report.txt
[Thread 3] Finished demo/basics.txt: 0 occurrences on 0 of 2 lines

Processing summary
demo/sensors.txt: Completed
demo/network.txt: Completed
demo/basics.txt: Completed
```

### Supporting files

- Sensors with two hits on one line ([input](demo/sensors.txt), [report](demo/sensors_report.txt))
- Network with one lowercase hit ([input](demo/network.txt), [report](demo/network_report.txt))
- C basics with zero hits ([input](demo/basics.txt), [report](demo/basics_report.txt))

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
