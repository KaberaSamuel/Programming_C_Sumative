# Student Academic Records and Performance Analytics

I use this program to keep student details, register courses, and check academic results.

## Demonstration

I use three fictional records and add a fourth to try the editing options.

From this folder, build the program and copy the sample records to a temporary file.

```bash
demo_dir=$(mktemp -d)
cp demo/students.txt "$demo_dir/students.txt"
gcc -std=c11 -Wall -Wextra -Wpedantic records.c -o "$demo_dir/records"
"$demo_dir/records" "$demo_dir/students.txt"
```

1. Choose **1**. Enter ID `104`, first name `Sam`, last name `Demo`, programme `Computer Science`, and year `1`.
2. Choose **6** to add `CSC102` (`Data Structures`) to student `104` with a maximum of `100` and a mark of `70`. Choose **3** to view the result. Use **7** to change the mark to `75`, then view the result again.
3. Choose **8**, search by name for `Demo`, and choose **9** to sort by ID. Choose **5** to delete student `104` and confirm with `y`.
4. Exit with **0** and run the last command again. Choose **10** to show the academic report for the three saved records.

![VS Code terminal showing the academic report after reloading three students](demo/terminal.png)

### Terminal output

```text
Loaded 3 students.

Academic report
Students: 3
Course registrations: 2
Passing: 1
Failing: 1
No courses: 1
Overall average: 63.0%
Highest: Amina Kato (ID 101), 84.0%
Lowest: Isaac Musa (ID 102), 42.0%

Average by course code
CSC101: 63.0% (2 records)

Students per programme
Information Technology: 2
Computer Science: 1
```

### Supporting files

- [Sample student records](demo/students.txt)
- [Academic report output](demo/academic-report.txt)

## Build and run

From this folder:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic records.c -o records
./records
```

The program loads `students.txt` at startup and saves after each edit.

## How it works

- I store students in a dynamic array. Each student has a separate array of courses. Pointers give the menu functions access to the student or course they need. The arrays grow with `realloc` and are freed when the program closes.
- I search by checking each student in order. I use selection sort to put the students in the requested order. Name and programme searches match part of the text and are case-sensitive.
- I divide total marks earned by total possible marks for each student. A student passes only if every course reaches 50%. The report counts students with no courses separately. Course averages use the percentage from each student's record for that course code.
- The file uses a student row followed by that student's course rows. I check IDs, marks, and the file format when loading. If a save fails, the previous file stays in place and the program reports the error.
