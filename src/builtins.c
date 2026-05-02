#include "executables.h"
#include <dirent.h>
#include <errno.h>
#include <readline/history.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char* builtins[] = {"echo", "exit", "type", "pwd", "cd", "history", "jobs", NULL};
int history_entries_saved = 0;

int is_builtin(char* name) {
    for (int i = 0; builtins[i] != NULL; i++)
        if (strcmp(name, builtins[i]) == 0) return 1;
    return 0;
}

static void handle_type(char** argv) {
    if (argv[1] == NULL || strcmp(argv[1], "") == 0) return;

    for (int i = 0; builtins[i] != NULL; i++) {
        if (strcmp(argv[1], builtins[i]) == 0) {
            printf("%s is a shell builtin\n", argv[1]);
            return;
        }
    }

    char* exec_path = find_executable(argv[1]);
    if (exec_path)
        printf("%s\n", exec_path);
    else
        fprintf(stderr, "%s: not found\n", argv[1]);
}

static void handle_history_options(char** argv) {
    if (
        strcmp(argv[1], "-r") != 0
        && strcmp(argv[1], "-w") != 0
        && strcmp(argv[1], "-a") != 0
    ) {
        fprintf(stderr, "history: %s: invalid option\n", argv[1]);
        return;
    }

    char* filename = argv[2];
    if (filename == NULL) {
        fprintf(stderr, "history: %s: filename must be specified\n", argv[1]);
        return;
    }

    char mode = argv[1][1];
    if (mode == 'r') {
        read_history(filename);
    } else if (mode == 'w') {
        write_history(filename);
    } else {
        append_history(history_length - history_entries_saved, filename);
        history_entries_saved = history_length;
    }
}

void execute_builtin_command(Command command) {
    if (strcmp(command.argv[0], "echo") == 0) {
        if (command.argv[1] != NULL) {
            printf("%s", command.argv[1]);
            for (int i = 2; command.argv[i] != NULL; i++) {
                printf(" %s", command.argv[i]);
            }
        }
        printf("\n");
    } else if (strcmp(command.argv[0], "type") == 0) {
        handle_type(command.argv);
    } else if (strcmp(command.argv[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
    } else if (strcmp(command.argv[0], "cd") == 0) {
        char* path;
        if (command.argv[1] == NULL) {
            path = strdup("~");
        } else if (strcmp(command.argv[1], "") == 0) {
            path = strdup(".");
        } else {
            path = strdup(command.argv[1]);
        }

        if (strncmp(path, "~", 1) == 0) {
            char* home = getenv("HOME");
            char* expanded = malloc(strlen(path) + strlen(home));
            strcpy(expanded, home);
            strcpy(expanded + strlen(home), path + 1);
            free(path);
            path = expanded;
        }
        DIR* d = opendir(path);
        if (d) {
            closedir(d);
            chdir(path);
        } else if (errno == ENOENT) {
            fprintf(stderr, "cd: %s: No such file or directory\n", path);
        }
        free(path);
    } else if (strcmp(command.argv[0], "history") == 0) {
        int limit = 1000;
        char* arg;
        if ((arg = command.argv[1]) != NULL) {
            if (arg[0] == '-') {
                handle_history_options(command.argv);
                return;
            } else {
                char* endptr;
                limit = strtol(arg, &endptr, 10);
                if (*endptr != '\0') {
                    fprintf(stderr, "history: %s: numeric argument required\n", arg);
                    return;
                }
            }
        }

        HIST_ENTRY** history = history_list();
        int start = limit >= history_length ? 0 : history_length - limit;
        for (int i = start; history[i] != NULL; i++) {
            printf("    %d  %s\n", i + 1, history[i]->line);
        }
    } else if (strcmp(command.argv[0], "jobs") == 0) {
    }
}
