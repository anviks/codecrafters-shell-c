#include <asm-generic/errno-base.h>
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef _WIN32
#define PATH_LIST_SEPARATOR ";"
#else
#define PATH_LIST_SEPARATOR ":"
#endif

char* find_executable(char* name) {
    char* path_env = strdup(getenv("PATH"));
    char *path, *path_state;
    path = strtok_r(path_env, PATH_LIST_SEPARATOR, &path_state);
    while (path != NULL) {
        DIR* d = opendir(path);
        struct dirent* dir;
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                char* fullpath = malloc(strlen(path) + strlen(name) + 2);
                snprintf(fullpath, strlen(path) + strlen(name) + 2, "%s/%s", path, name);

                if (strcmp(dir->d_name, name) == 0 && access(fullpath, X_OK) == 0) {
                    closedir(d);
                    free(path_env);
                    return fullpath;
                }
            }
            closedir(d);
        }
        path = strtok_r(NULL, PATH_LIST_SEPARATOR, &path_state);
    }
    free(path_env);
    return NULL;
}

typedef enum { NORMAL, IN_SINGLE_QUOTE, IN_DOUBLE_QUOTE } State;

int main() {
    // Flush after every printf
    setbuf(stdout, NULL);

    while (1) {
        printf("$ ");
        char command[1024];
        fgets(command, sizeof(command), stdin);
        command[strlen(command) - 1] = '\0';

        if (strcmp(command, "") == 0) continue;
        if (strcmp(command, "exit") == 0) break;

        State state = NORMAL;
        char** argv = malloc(1024 * sizeof(char*));
        int argv_i = 0;
        char* running_arg = malloc(1024);
        int running_arg_i = 0;
        for (int i = 0; command[i] != '\0'; i++) {
            char c = command[i];
            switch (state) {
                case NORMAL:
                    if (c == '\\') running_arg[running_arg_i++] = command[++i];
                    else if (c == '\'') state = IN_SINGLE_QUOTE;
                    else if (c == '"') state = IN_DOUBLE_QUOTE;
                    else if (c == ' ') {
                        if (running_arg_i > 0) {
                            running_arg[running_arg_i] = '\0';
                            argv[argv_i++] = strdup(running_arg);
                            running_arg_i = 0;
                        }
                    }
                    else {
                        running_arg[running_arg_i++] = c;
                    }
                    break;
                case IN_SINGLE_QUOTE:
                    if (c == '\'') state = NORMAL;
                    else {
                        running_arg[running_arg_i++] = c;
                    }
                    break;
                case IN_DOUBLE_QUOTE:
                    if (c == '\\') {
                        char next = command[++i];
                        running_arg[running_arg_i++] = next;
                        // if (next == 'n') {
                        //     running_arg[running_arg_i++] = '\n';
                        // } else {
                        //     running_arg[running_arg_i++] = next;
                        // }
                    }
                    else if (c == '"') state = NORMAL;
                    else {
                        running_arg[running_arg_i++] = c;
                    }
                    break;
            }
        }
        running_arg[running_arg_i] = '\0';
        argv[argv_i++] = strdup(running_arg);
        argv[argv_i] = NULL;

        if (strncmp(command, "echo ", 5) == 0) {
            printf("%s", argv[1]);
            for (int i = 2; argv[i] != NULL; i++) {
                printf(" %s", argv[i]);
            }
            printf("\n");
        } else if (strncmp(command, "type ", 5) == 0) {
            char* args = command + 5;
            if (
                strcmp(args, "echo") == 0
                || strcmp(args, "exit") == 0
                || strcmp(args, "type") == 0
                || strcmp(args, "pwd") == 0
                || strcmp(args, "cd") == 0
            ) {
                printf("%s is a shell builtin\n", args);
            } else {
                char* exec_path = find_executable(args);
                if (exec_path)
                    printf("%s\n", exec_path);
                else
                    printf("%s: not found\n", args);
            }
        } else if (strcmp(command, "pwd") == 0) {
            char cwd[PATH_MAX];
            getcwd(cwd, sizeof(cwd));
            printf("%s\n", cwd);
        } else if (strncmp(command, "cd ", 3) == 0) {
            char* path = strdup(command + 3);
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
                printf("cd: %s: No such file or directory\n", path);
            }
            free(path);
        } else {
            char *arg, *arg_state;
            arg = strtok_r(strdup(command), " ", &arg_state);
            char* exec_path = find_executable(arg);
            if (!exec_path) {
                printf("%s: command not found\n", command);
                continue;
            }

            pid_t pid = fork();

            if (pid == 0) {
                execv(exec_path, argv);
                exit(127);
            } else {
                waitpid(pid, 0, 0);
            }

            free(exec_path);
        }
    }

    return 0;
}
