#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

static void scan_document(FileTask *task, FILE *input)
{
    char *line = NULL;
    size_t capacity = 0;
    size_t keyword_length = strlen(task->keyword);
    ssize_t bytes;
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
        }
    }
    if (ferror(input) || !feof(input))
        task->error = "Could not read the input file.";
    free(line);
}

static void *process_file(void *argument)
{
    FileTask *task = argument;
    printf("[Thread %d] Processing %s\n", task->number, task->path);

    FILE *input = fopen(task->path, "r");
    if (input) {
        scan_document(task, input);
        if (fclose(input) != 0 && !task->error)
            task->error = "Could not close the input file.";
    } else {
        task->error = "Could not open the input file.";
    }

    if (task->error) {
        printf("[Thread %d] %s: %s\n", task->number, task->path, task->error);
    } else {
        printf("[Thread %d] Finished %s: %zu occurrences on %zu of %zu lines\n",
               task->number, task->path, task->occurrences, task->matching_lines, task->lines);
    }
    return NULL;
}

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
        task->number = i + 1;
        task->path = argv[i + 2];
        task->keyword = argv[1];
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

    int result = EXIT_SUCCESS;
    puts("\nProcessing summary");
    for (int i = 0; i < count; i++) {
        FileTask *task = &tasks[i];
        if (task->error) {
            printf("%s: Failed (%s)\n", task->path, task->error);
            result = EXIT_FAILURE;
        } else {
            printf("%s: Completed\n", task->path);
        }
    }
    free(tasks);
    return result;
}
