#include "builtins.h"
#include "array.h"
#include "executables.h"
#include "jobs.h"
#include "completions.h"
#include <dirent.h>
#include <errno.h>
#include <readline/history.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char* builtins[] = {"echo", "exit", "type", "pwd", "cd", "history", "jobs", "complete", "declare", NULL};
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

static void handle_complete(char** argv) {
    if (argv[1] == NULL || argv[2] == NULL) return;

    if (strcmp(argv[1], "-p") == 0) {
        Completion* completion = array_find(&completions, argv[2]);
        if (completion == NULL) {
            fprintf(stderr, "complete: %s: no completion specification\n", argv[2]);
        } else {
            printf("complete -C '%s' %s\n", completion->completer, completion->command);
        }
    } else if (strcmp(argv[1], "-C") == 0) {
        if (argv[3] == NULL) return;
        array_add(&completions, &(Completion){
            .command = strdup(argv[3]),
            .completer = strdup(argv[2])
        });
    } else if (strcmp(argv[1], "-r") == 0) {
        array_remove(&completions, argv[2]);
    }
}

Array variables;

static void handle_declare(char** argv) {
    if (argv[1] == NULL) return;

    if (strcmp(argv[1], "-p") == 0) {
        if (argv[2] == NULL) return;
        Variable* variable = array_find(&variables, argv[2]);
        if (!variable) fprintf(stderr, "declare: %s: not found\n", argv[2]);
        else printf("declare -- %s=\"%s\"\n", variable->name, variable->value);
    } else {
        char* eq = strchr(argv[1], '=');
        char* name = strndup(argv[1], eq - argv[1]);
        char* value = strdup(eq + 1);
        array_remove(&variables, name);
        array_add(&variables, &(Variable){.name = name, .value = value});
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
        for (int i = 0; i < job_count; i++) {
            print_job(i);
        }
        delete_done_jobs();
    } else if (strcmp(command.argv[0], "complete") == 0) {
        handle_complete(command.argv);
    } else if (strcmp(command.argv[0], "declare") == 0) {
        handle_declare(command.argv);
    }
}
