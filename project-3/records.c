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

static void show_menu(void)
{
    puts("\nStudent Academic Records\n"
         "1. Register a student\n2. List students\n3. View student and course results\n"
         "4. Update student details\n5. Delete a student\n6. Add a course\n"
         "7. Update a course mark\n0. Exit");
}

int main(void)
{
    Register records = {0};
    while (!feof(stdin) && !ferror(stdin)) {
        int choice, changed = 0;
        show_menu();
        if (!ask_number("Choose an option: ", 0, 7, &choice))
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
        }
        if (changed)
            puts("Record updated.");
    }
    free_register(&records);
    return 0;
}
