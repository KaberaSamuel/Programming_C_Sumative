#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

typedef struct {
    int number;
    const char *path;
    const char *keyword;
    pthread_t thread;
    int started;
    const char *error;
    size_t lines;
    size_t occurrences;
    size_t matching_lines;
} FileTask;

/* Count keyword hits in a line, ignoring case and overlaps. */
static size_t count_matches(const char *line, size_t length, const char *keyword,
                            size_t keyword_length)
{
    size_t count = 0;
    if (keyword_length > length)
        return 0;
    for (size_t i = 0; i <= length - keyword_length;) {
        if (strncasecmp(line + i, keyword, keyword_length) == 0) {
            count++;
            i += keyword_length;
        } else {
            i++;
        }
    }
    return count;
}

/* Collect line totals and record matching line numbers. */
static int scan_document(FileTask *task, FILE *input, FILE *report)
{
    char *line = NULL;
    size_t capacity = 0;
    size_t keyword_length = strlen(task->keyword);
    ssize_t bytes;
    int written = 1;
    while ((bytes = getline(&line, &capacity, input)) >= 0) {
        size_t length = (size_t)bytes;
        if (length > 0 && line[length - 1] == '\n')
            length--;
        if (length > 0 && line[length - 1] == '\r')
            length--;

        task->lines++;
        size_t matches = count_matches(line, length, task->keyword, keyword_length);
        if (matches > 0) {
            task->matching_lines++;
            task->occurrences += matches;
            if (fprintf(report, "%zu\n", task->lines) < 0) {
                written = 0;
                break;
            }
        }
    }
    if (written && (ferror(input) || !feof(input)))
        task->error = "Could not read the input file.";
    free(line);
    return written;
}

/* Create a new report without overwriting an existing file. */
static FILE *open_report(const char *path, char **report_name)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (*base == '\0')
        base = "document";
    const char *extension = strrchr(base, '.');
    size_t length = extension && extension != base ? (size_t)(extension - base) : strlen(base);

    const size_t suffix_size = 40;
    char *name = malloc(length + suffix_size);
    if (!name)
        return NULL;
    memcpy(name, base, length);

    /* Reserve the name without overwriting another file or another thread's report. */
    for (size_t number = 1; ; number++) {
        if (number == 1)
            snprintf(name + length, suffix_size, "_report.txt");
        else
            snprintf(name + length, suffix_size, "_report_%zu.txt", number);

        FILE *report = fopen(name, "wx");
        if (report) {
            *report_name = name;
            return report;
        }
        if (errno != EEXIST) {
            free(name);
            return NULL;
        }
    }
}

/* Check that the input is a readable regular file, then scan it. */
static int analyze_input(FileTask *task, FILE *report)
{
    struct stat info;
    if (stat(task->path, &info) != 0) {
        task->error = "Could not open the input file.";
        return 1;
    }
    if (!S_ISREG(info.st_mode)) {
        task->error = "Input must be a regular file.";
        return 1;
    }

    FILE *input = fopen(task->path, "r");
    if (!input) {
        task->error = "Could not open the input file.";
        return 1;
    }
    int written = scan_document(task, input, report);
    if (fclose(input) != 0 && !task->error)
        task->error = "Could not close the input file.";
    return written;
}

/* Write the findings and final status for one file. */
static int write_report(FileTask *task, FILE *report)
{
    if (fprintf(report, "Input file: %s\nKeyword: %s\n\nMatching line numbers:\n",
                task->path, task->keyword) < 0 || !analyze_input(task, report))
        return 0;
    if (task->matching_lines == 0 && fputs("None\n", report) == EOF)
        return 0;
    if (fprintf(report, "\nTotal lines: %zu\nTotal occurrences: %zu\nMatching lines: %zu\n"
                "Processing status: %s\n", task->lines, task->occurrences,
                task->matching_lines, task->error ? "Failed" : "Successful") < 0)
        return 0;
    return !task->error || fprintf(report, "Error: %s\n", task->error) >= 0;
}

/* Do one worker's full job on its own file. */
static void *process_file(void *argument)
{
    FileTask *task = argument;
    char *name = NULL;
    printf("[Thread %d] Processing %s\n", task->number, task->path);

    FILE *report = open_report(task->path, &name);
    if (report) {
        int written = write_report(task, report);
        if (fclose(report) != 0 || !written) {
            task->error = "Could not write the report.";
            if (remove(name) != 0)
                printf("[Thread %d] Could not remove incomplete report %s\n", task->number, name);
        } else {
            printf("[Thread %d] Saved %s\n", task->number, name);
        }
    } else {
        task->error = "Could not create the report.";
    }

    if (task->error) {
        printf("[Thread %d] %s: %s\n", task->number, task->path, task->error);
    } else {
        printf("[Thread %d] Finished %s: %zu occurrences on %zu of %zu lines\n",
               task->number, task->path, task->occurrences, task->matching_lines, task->lines);
    }
    free(name);
    return NULL;
}

/* Give one worker its number, input file and keyword. */
static void init_task(FileTask *task, int number, const char *path, const char *keyword)
{
    task->number = number;
    task->path = path;
    task->keyword = keyword;
}

/* Show each file's outcome and report whether all of them succeeded. */
static int show_summary(const FileTask *tasks, int count)
{
    int result = EXIT_SUCCESS;
    puts("\nProcessing summary");
    for (int i = 0; i < count; i++) {
        const FileTask *task = &tasks[i];
        if (task->error) {
            printf("%s: Failed (%s)\n", task->path, task->error);
            result = EXIT_FAILURE;
        } else {
            printf("%s: Completed\n", task->path);
        }
    }
    return result;
}

/* Start one thread per file and wait for all of them. */
int main(int argc, char **argv)
{
    if (argc < 3 || argv[1][0] == '\0') {
        fprintf(stderr, "Usage: %s keyword file [file ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int count = argc - 2;
    FileTask *tasks = calloc((size_t)count, sizeof *tasks);
    if (!tasks) {
        fputs("Could not allocate memory for the file tasks.\n", stderr);
        return EXIT_FAILURE;
    }

    printf("Document Search\nKeyword: %s\nInput files: %d\n\n", argv[1], count);
    for (int i = 0; i < count; i++) {
        FileTask *task = &tasks[i];
        init_task(task, i + 1, argv[i + 2], argv[1]);
        printf("[Thread %d] Creating worker for %s\n", task->number, task->path);
        int status = pthread_create(&task->thread, NULL, process_file, task);
        if (status == 0) {
            task->started = 1;
        } else {
            task->error = "Could not create the thread.";
            fprintf(stderr, "[Thread %d] %s: %s\n", task->number, task->error, strerror(status));
        }
    }

    int join_failed = 0;
    for (int i = 0; i < count; i++) {
        FileTask *task = &tasks[i];
        if (task->started) {
            int status = pthread_join(task->thread, NULL);
            if (status != 0) {
                fprintf(stderr, "[Thread %d] Could not join the thread: %s\n", i + 1, strerror(status));
                join_failed = 1;
            }
        }
    }
    if (join_failed)
        return EXIT_FAILURE;

    int result = show_summary(tasks, count);
    free(tasks);
    return result;
}
