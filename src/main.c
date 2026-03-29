#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#ifdef _WIN32
#define PATH_LIST_SEPARATOR ";"
#else
#define PATH_LIST_SEPARATOR ":"
#endif

static char* builtins[] = {"echo", "exit", "type", "pwd", "cd", NULL};

int cmp(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

char** find_executable_completions(char* name) {
    char** result = malloc(1024 * sizeof(char*));
    int i = 0;
    char* path_env = strdup(getenv("PATH"));
    char *path, *path_state;
    path = strtok_r(path_env, PATH_LIST_SEPARATOR, &path_state);
    while (path != NULL) {
        DIR* d = opendir(path);
        struct dirent* dir;
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                if (strncmp(dir->d_name, name, strlen(name)) != 0) continue;

                char* fullpath = malloc(strlen(path) + strlen(dir->d_name) + 2);
                snprintf(fullpath, strlen(path) + strlen(dir->d_name) + 2, "%s/%s", path, dir->d_name);

                if (access(fullpath, X_OK) != 0) {
                    free(fullpath);
                    continue;
                }
                free(fullpath);
                result[i++] = strdup(dir->d_name);
            }
            closedir(d);
        }
        path = strtok_r(NULL, PATH_LIST_SEPARATOR, &path_state);
    }
    free(path_env);
    qsort(result, i, sizeof(char*), cmp);
    result[i] = NULL;

    return result;
}

char* find_executable(char* name) {
    char* path_env = strdup(getenv("PATH"));
    char *path, *path_state;
    path = strtok_r(path_env, PATH_LIST_SEPARATOR, &path_state);
    while (path != NULL) {
        DIR* d = opendir(path);
        struct dirent* dir;
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                if (strcmp(dir->d_name, name) != 0) continue;

                char* fullpath = malloc(strlen(path) + strlen(dir->d_name) + 2);
                snprintf(fullpath, strlen(path) + strlen(dir->d_name) + 2, "%s/%s", path, dir->d_name);

                if (access(fullpath, X_OK) != 0) {
                    free(fullpath);
                    continue;
                }

                closedir(d);
                free(path_env);
                return fullpath;
            }
            closedir(d);
        }
        path = strtok_r(NULL, PATH_LIST_SEPARATOR, &path_state);
    }
    free(path_env);
    return NULL;
}

static char* completion_generator(const char* text, int state) {
    static int builtin_idx;
    static char** exec_matches;
    static int exec_idx;

    if (state == 0) {
        builtin_idx = 0;
        exec_matches = find_executable_completions((char*)text);
        exec_idx = 0;
    }

    while (builtins[builtin_idx] != NULL) {
        char* b = builtins[builtin_idx++];
        if (strncmp(b, text, strlen(text)) == 0)
            return strdup(b);
    }

    if (exec_matches) {
        while (exec_matches[exec_idx] != NULL)
            return exec_matches[exec_idx++];
    }

    return NULL;
}

static char** shell_completion(const char* text, int start, int end) {
    (void)end;
    rl_attempted_completion_over = 1;
    if (start != 0) return NULL;
    return rl_completion_matches(text, completion_generator);
}

void handle_type(char** argv) {
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
        printf("%s: not found\n", argv[1]);
}

typedef enum { NORMAL, IN_SINGLE_QUOTE, IN_DOUBLE_QUOTE } State;
typedef enum { NONE, STDOUT, STDERR, APPEND_STDOUT, APPEND_STDERR } RedirectMode;

int main() {
    setbuf(stdout, NULL);  // Flush after every printf
    rl_attempted_completion_function = shell_completion;

    char* command;
    while ((command = readline("$ ")) != NULL) {
        if (*command) add_history(command);

        State state = NORMAL;
        RedirectMode redirect = NONE;
        char* redirect_file;
        char** argv = malloc(1024 * sizeof(char*));
        char* running_arg = malloc(1024);
        int argv_i = 0, running_arg_i = 0;

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
                            if (strcmp(running_arg, ">") == 0 || strcmp(running_arg, "1>") == 0) {
                                redirect = STDOUT;
                            } else if (strcmp(running_arg, "2>") == 0) {
                                redirect = STDERR;
                            } else if (strcmp(running_arg, ">>") == 0 || strcmp(running_arg, "1>>") == 0) {
                                redirect = APPEND_STDOUT;
                            } else if (strcmp(running_arg, "2>>") == 0) {
                                redirect = APPEND_STDERR;
                            } else if (redirect == NONE) {
                                argv[argv_i++] = strdup(running_arg);
                            } else {
                                redirect_file = strdup(running_arg);
                            }
                            running_arg_i = 0;
                        }
                    }
                    else running_arg[running_arg_i++] = c;
                    break;
                case IN_SINGLE_QUOTE:
                    if (c == '\'') state = NORMAL;
                    else running_arg[running_arg_i++] = c;
                    break;
                case IN_DOUBLE_QUOTE:
                    if (c == '\\') running_arg[running_arg_i++] = command[++i];
                    else if (c == '"') state = NORMAL;
                    else running_arg[running_arg_i++] = c;
                    break;
            }
        }

        running_arg[running_arg_i] = '\0';
        if (redirect == NONE) {
            argv[argv_i++] = strdup(running_arg);
        } else {
            redirect_file = strdup(running_arg);
        }
        argv[argv_i] = NULL;

        free(command);
        free(running_arg);

        if (argv[0] == NULL || strcmp(argv[0], "") == 0) continue;

        if (strcmp(argv[0], "exit") == 0) break;

        if (redirect == STDOUT) {
            freopen(redirect_file, "w", stdout);
        } else if (redirect == STDERR) {
            freopen(redirect_file, "w", stderr);
        } else if (redirect == APPEND_STDOUT) {
            freopen(redirect_file, "a", stdout);
        } else if (redirect == APPEND_STDERR) {
            freopen(redirect_file, "a", stderr);
        }

        if (redirect != NONE) free(redirect_file);

        if (strcmp(argv[0], "echo") == 0) {
            printf("%s", argv[1]);
            for (int i = 2; argv[i] != NULL; i++) {
                printf(" %s", argv[i]);
            }
            printf("\n");
        } else if (strcmp(argv[0], "type") == 0) {
            handle_type(argv);
        } else if (strcmp(argv[0], "pwd") == 0) {
            char cwd[PATH_MAX];
            getcwd(cwd, sizeof(cwd));
            printf("%s\n", cwd);
        } else if (strcmp(argv[0], "cd") == 0) {
            char* path;
            if (argv[1] == NULL) {
                path = strdup("~");
            } else if (strcmp(argv[1], "") == 0) {
                path = strdup(".");
            } else {
                path = strdup(argv[1]);
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
                printf("cd: %s: No such file or directory\n", path);
            }
            free(path);
        } else {
            char* exec_path = find_executable(argv[0]);
            if (!exec_path) {
                printf("%s: command not found\n", argv[0]);
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

        if (redirect == STDOUT || redirect == APPEND_STDOUT) {
            freopen("/dev/tty", "w", stdout);
            setbuf(stdout, NULL);  // Flush after every printf
        } else if (redirect == STDERR || redirect == APPEND_STDERR) {
            freopen("/dev/tty", "w", stderr);
        }
    }

    return 0;
}
