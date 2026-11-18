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
    char code[TEXT_SIZE];
    char name[TEXT_SIZE];
    int mark;
    int maximum;
} Course;

typedef struct {
    int id;
    char first[TEXT_SIZE];
    char last[TEXT_SIZE];
    char programme[TEXT_SIZE];
    int year;
    Course *courses;
    size_t course_count;
} Student;

typedef struct {
    Student *students;
    size_t count;
} Register;

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

static Student *find_student(Register *records, int id)
{
    for (size_t i = 0; i < records->count; i++) {
        if (records->students[i].id == id)
            return &records->students[i];
    }
    return NULL;
}

static Course *find_course(Student *student, const char *code)
{
    for (size_t i = 0; i < student->course_count; i++) {
        if (strcmp(student->courses[i].code, code) == 0)
            return &student->courses[i];
    }
    return NULL;
}

static Student *select_student(Register *records)
{
    int id;
    if (!ask_number("Student ID: ", 1, INT_MAX, &id))
        return NULL;
    Student *student = find_student(records, id);
    if (!student)
        puts("Student not found.");
    return student;
}

static int append_student(Register *records, Student student)
{
    size_t bytes = (records->count + 1) * sizeof student;
    Student *students = records->students ? realloc(records->students, bytes) : malloc(bytes);
    if (!students) {
        puts("Could not allocate memory for the student.");
        return 0;
    }
    records->students = students;
    records->students[records->count++] = student;
    return 1;
}

static int read_profile(Student *student)
{
    return ask_text("First name: ", student->first) &&
           ask_text("Last name: ", student->last) &&
           ask_text("Programme: ", student->programme) &&
           ask_number("Year: ", 1, INT_MAX, &student->year);
}

static int register_student(Register *records)
{
    Student student = {0};
    if (!ask_number("New student ID: ", 1, INT_MAX, &student.id))
        return 0;
    if (find_student(records, student.id)) {
        puts("That student ID is already registered.");
        return 0;
    }
    return read_profile(&student) && append_student(records, student);
}

static void show_student(const Student *student)
{
    printf("%d | %s %s | %s | Year %d | %zu courses\n",
           student->id, student->first, student->last, student->programme,
           student->year, student->course_count);
}

static void list_students(const Register *records)
{
    if (records->count == 0)
        puts("No students registered.");
    for (size_t i = 0; i < records->count; i++)
        show_student(&records->students[i]);
}

static double course_grade(const Course *course)
{
    return 100.0 * course->mark / course->maximum;
}

static double student_average(const Student *student)
{
    double earned = 0;
    double possible = 0;
    for (size_t i = 0; i < student->course_count; i++) {
        earned += student->courses[i].mark;
        possible += student->courses[i].maximum;
    }
    return possible > 0 ? 100.0 * earned / possible : 0;
}

static void show_details(Register *records)
{
    Student *student = select_student(records);
    if (!student)
        return;
    show_student(student);
    for (size_t i = 0; i < student->course_count; i++) {
        Course *course = &student->courses[i];
        printf("%s | %s | %d/%d | %.1f%% | %s\n", course->code, course->name,
               course->mark, course->maximum, course_grade(course),
               course_grade(course) >= 50 ? "Pass" : "Fail");
    }
    if (student->course_count)
        printf("Overall average: %.1f%%\n", student_average(student));
    else
        puts("No courses registered.");
}

static int update_student(Register *records)
{
    Student *student = select_student(records);
    if (!student)
        return 0;
    Student updated = *student;
    if (!read_profile(&updated))
        return 0;
    *student = updated;
    return 1;
}

static int delete_student(Register *records)
{
    Student *student = select_student(records);
    char answer[LINE_SIZE];
    if (!student || prompt("Delete this student? [y/N] ", answer) != 1 ||
        strcmp(answer, "y") != 0)
        return 0;
    size_t index = (size_t)(student - records->students);
    free(student->courses);
    records->count--;
    for (size_t i = index; i < records->count; i++)
        records->students[i] = records->students[i + 1];
    if (records->count == 0) {
        free(records->students);
        records->students = NULL;
    } else {
        Student *smaller = realloc(records->students, records->count * sizeof *smaller);
        if (smaller)
            records->students = smaller;
    }
    return 1;
}

static int append_course(Student *student, Course course)
{
    Course *courses = realloc(student->courses, (student->course_count + 1) * sizeof course);
    if (!courses) {
        puts("Could not allocate memory for the course.");
        return 0;
    }
    student->courses = courses;
    student->courses[student->course_count++] = course;
    return 1;
}

static int add_course(Register *records)
{
    Student *student = select_student(records);
    Course course;
    if (!student || !ask_text("Course code: ", course.code))
        return 0;
    if (find_course(student, course.code)) {
        puts("That course is already registered for this student.");
        return 0;
    }
    if (!ask_text("Course name: ", course.name) ||
        !ask_number("Maximum marks: ", 1, INT_MAX, &course.maximum) ||
        !ask_number("Mark obtained: ", 0, course.maximum, &course.mark))
        return 0;
    return append_course(student, course);
}

static int update_mark(Register *records)
{
    Student *student = select_student(records);
    char code[TEXT_SIZE];
    if (!student || !ask_text("Course code: ", code))
        return 0;
    Course *course = find_course(student, code);
    if (!course) {
        puts("Course not found.");
        return 0;
    }
    return ask_number("New mark: ", 0, course->maximum, &course->mark);
}

static void free_register(Register *records)
{
    for (size_t i = 0; i < records->count; i++)
        free(records->students[i].courses);
    free(records->students);
    *records = (Register){0};
}

static void search_students(Register *records)
{
    int choice;
    if (!ask_number("1. ID  2. First or last name  3. Programme: ", 1, 3, &choice))
        return;
    if (choice == 1) {
        Student *student = select_student(records);
        if (student)
            show_student(student);
        return;
    }
    char query[TEXT_SIZE];
    if (!ask_text("Search text: ", query))
        return;
    int found = 0;
    for (size_t i = 0; i < records->count; i++) {
        Student *student = &records->students[i];
        int match;
        if (choice == 2)
            match = strstr(student->first, query) || strstr(student->last, query);
        else
            match = strstr(student->programme, query) != NULL;
        if (match) {
            show_student(student);
            found = 1;
        }
    }
    if (!found)
        puts("No matching students.");
}

static int comes_before(const Student *a, const Student *b, int order)
{
    switch (order) {
    case 1: return a->id < b->id;
    case 2: {
        int last = strcmp(a->last, b->last);
        return last < 0 || (last == 0 && strcmp(a->first, b->first) < 0);
    }
    case 3:
        if (!a->course_count || !b->course_count)
            return a->course_count && !b->course_count;
        return student_average(a) > student_average(b);
    default: return strcmp(a->programme, b->programme) < 0;
    }
}

static void sort_students(Register *records)
{
    int order;
    if (!ask_number("1. ID  2. Name  3. Average  4. Programme: ", 1, 4, &order))
        return;
    for (size_t i = 0; i < records->count; i++) {
        size_t best = i;
        for (size_t j = i + 1; j < records->count; j++) {
            if (comes_before(&records->students[j], &records->students[best], order))
                best = j;
        }
        Student saved = records->students[i];
        records->students[i] = records->students[best];
        records->students[best] = saved;
    }
    list_students(records);
}

static int passes_all_courses(const Student *student)
{
    for (size_t i = 0; i < student->course_count; i++) {
        if (course_grade(&student->courses[i]) < 50)
            return 0;
    }
    return 1;
}

static void report_programmes(const Register *records)
{
    puts("\nStudents per programme");
    for (size_t i = 0; i < records->count; i++) {
        const char *programme = records->students[i].programme;
        size_t first = 0, count = 0;
        for (size_t j = 0; j < records->count; j++) {
            if (strcmp(records->students[j].programme, programme) == 0) {
                if (count == 0)
                    first = j;
                count++;
            }
        }
        if (first == i)
            printf("%s: %zu\n", programme, count);
    }
}

static void report_courses(Register *records)
{
    puts("\nAverage by course code");
    for (size_t i = 0; i < records->count; i++) {
        Student *student = &records->students[i];
        for (size_t j = 0; j < student->course_count; j++) {
            const char *code = student->courses[j].code;
            size_t first = 0, count = 0;
            double sum = 0;
            for (size_t k = 0; k < records->count; k++) {
                Course *course = find_course(&records->students[k], code);
                if (course) {
                    if (count == 0)
                        first = k;
                    count++;
                    sum += course_grade(course);
                }
            }
            if (first == i)
                printf("%s: %.1f%% (%zu records)\n", code, sum / count, count);
        }
    }
}

static void academic_report(Register *records)
{
    size_t courses = 0, graded = 0, passed = 0;
    const Student *highest = NULL, *lowest = NULL;
    double total = 0, high = 0, low = 0;
    for (size_t i = 0; i < records->count; i++) {
        const Student *student = &records->students[i];
        courses += student->course_count;
        if (!student->course_count)
            continue;
        double average = student_average(student);
        total += average;
        if (!highest || average > high) {
            highest = student;
            high = average;
        }
        if (!lowest || average < low) {
            lowest = student;
            low = average;
        }
        graded++;
        passed += passes_all_courses(student);
    }
    printf("\nAcademic report\nStudents: %zu\nCourse registrations: %zu\n",
           records->count, courses);
    printf("Passing: %zu\nFailing: %zu\nNo courses: %zu\n",
           passed, graded - passed, records->count - graded);
    if (graded) {
        printf("Overall average: %.1f%%\n", total / graded);
        printf("Highest: %s %s (ID %d), %.1f%%\n",
               highest->first, highest->last, highest->id, high);
        printf("Lowest: %s %s (ID %d), %.1f%%\n",
               lowest->first, lowest->last, lowest->id, low);
        report_courses(records);
    } else {
        puts("No marks available.");
    }
    if (records->count)
        report_programmes(records);
}

/* Split a saved row into exactly the expected number of fields. */
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

static int load_course(FILE *file, Student *student)
{
    char line[LINE_SIZE], *field[5];
    Course course;
    if (read_line(file, line, sizeof line) != 1 || !split_row(line, field, 5) ||
        strcmp(field[0], "C") != 0 || !valid_text(field[1]) || !valid_text(field[2]) ||
        !parse_number(field[4], 1, INT_MAX, &course.maximum) ||
        !parse_number(field[3], 0, course.maximum, &course.mark) ||
        find_course(student, field[1]))
        return 0;
    strcpy(course.code, field[1]);
    strcpy(course.name, field[2]);
    return append_course(student, course);
}

static int load_student(FILE *file, char *line, Register *records)
{
    char *field[7];
    Student student = {0};
    int courses;
    if (!split_row(line, field, 7) || strcmp(field[0], "S") != 0 ||
        !parse_number(field[1], 1, INT_MAX, &student.id) ||
        !valid_text(field[2]) || !valid_text(field[3]) || !valid_text(field[4]) ||
        !parse_number(field[5], 1, INT_MAX, &student.year) ||
        !parse_number(field[6], 0, INT_MAX, &courses) ||
        find_student(records, student.id))
        return 0;
    strcpy(student.first, field[2]);
    strcpy(student.last, field[3]);
    strcpy(student.programme, field[4]);
    if (!append_student(records, student))
        return 0;
    Student *added = &records->students[records->count - 1];
    for (int i = 0; i < courses; i++) {
        if (!load_course(file, added))
            return 0;
    }
    return 1;
}

static int load_records(const char *path, Register *records)
{
    FILE *file = fopen(path, "r");
    if (!file) {
        if (errno == ENOENT)
            return 1;
        perror(path);
        return 0;
    }
    Register loaded = {0};
    char line[LINE_SIZE];
    int status;
    while ((status = read_line(file, line, sizeof line)) == 1) {
        if (!load_student(file, line, &loaded)) {
            status = -1;
            break;
        }
    }
    int failed = status < 0 || ferror(file);
    if (fclose(file) != 0)
        failed = 1;
    if (failed) {
        fprintf(stderr, "Could not load student records from %s. Check the file and its format.\n", path);
        free_register(&loaded);
        return 0;
    }
    free_register(records);
    *records = loaded;
    return 1;
}

static int write_records(FILE *file, const Register *records)
{
    for (size_t i = 0; i < records->count; i++) {
        const Student *student = &records->students[i];
        if (fprintf(file, "S|%d|%s|%s|%s|%d|%zu\n", student->id, student->first,
                    student->last, student->programme, student->year, student->course_count) < 0)
            return 0;
        for (size_t j = 0; j < student->course_count; j++) {
            const Course *course = &student->courses[j];
            if (fprintf(file, "C|%s|%s|%d|%d\n", course->code, course->name,
                        course->mark, course->maximum) < 0)
                return 0;
        }
    }
    return 1;
}

static int save_records(const char *path, const Register *records)
{
    const char suffix[] = ".tmp.XXXXXX";
    char *temporary = malloc(strlen(path) + sizeof suffix);
    if (!temporary) {
        fputs("Could not allocate memory to save records.\n", stderr);
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
        int written = write_records(file, records);
        int closed = fclose(file) == 0;
        saved = written && closed && rename(temporary, path) == 0;
    } else {
        close(fd);
    }
    if (!saved) {
        fprintf(stderr, "Could not save student records to %s.\n", path);
        remove(temporary);
    }
    free(temporary);
    return saved;
}

static void show_menu(void)
{
    puts("\nStudent Academic Records\n"
         "1. Register a student\n2. List students\n3. View student and course results\n"
         "4. Update student details\n5. Delete a student\n6. Add a course\n"
         "7. Update a course mark\n8. Search students\n9. Sort students\n"
         "10. Show academic report\n0. Exit");
}

int main(int argc, char **argv)
{
    if (argc > 2) {
        fputs("Usage: ./records [data-file]\n", stderr);
        return 1;
    }
    const char *path = argc == 2 ? argv[1] : "students.txt";
    Register records = {0};
    if (!load_records(path, &records))
        return 1;
    printf("Loaded %zu students.\n", records.count);
    int unsaved = 0;
    while (!feof(stdin) && !ferror(stdin)) {
        int choice, changed = 0;
        show_menu();
        if (!ask_number("Choose an option: ", 0, 10, &choice))
            continue;
        if (choice == 0)
            break;
        switch (choice) {
        case 1: changed = register_student(&records); break;
        case 2: list_students(&records); break;
        case 3: show_details(&records); break;
        case 4: changed = update_student(&records); break;
        case 5: changed = delete_student(&records); break;
        case 6: changed = add_course(&records); break;
        case 7: changed = update_mark(&records); break;
        case 8: search_students(&records); break;
        case 9: sort_students(&records); break;
        case 10: academic_report(&records); break;
        }
        if (changed) {
            unsaved = !save_records(path, &records);
            puts(unsaved ? "Record updated in memory. Changes are unsaved." : "Record saved.");
        }
    }
    if (unsaved && !save_records(path, &records)) {
        fputs("Changes could not be saved.\n", stderr);
        free_register(&records);
        return 1;
    }
    free_register(&records);
    return 0;
}
