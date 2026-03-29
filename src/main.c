#include <asm-generic/errno-base.h>
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>

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

                if (access(fullpath, X_OK) != 0) continue;
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

                if (access(fullpath, X_OK) != 0) continue;

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

static struct termios orig_termios;

void enable_raw_mode(void) {
    tcgetattr(0, &orig_termios);  // Save current settings
    struct termios raw_termios = orig_termios;  // Copy to modify
    raw_termios.c_lflag &= ~(ICANON | ECHO);  // Turn off line buffering and echo
    tcsetattr(0, TCSAFLUSH, &raw_termios);
}

void disable_raw_mode(void) {
    tcsetattr(0, TCSAFLUSH, &orig_termios);
}

void complete_with(char* command, int* index, const char* match) {
    printf("%s ", match + *index);
    *index = strlen(match);
    strcpy(command, match);
    command[(*index)++] = ' ';
}

void handle_tab(char* command, int* index, int is_double) {
    for (int j = 0; builtins[j] != NULL; j++) {
        if (strncmp(command, builtins[j], *index) == 0) {
            return complete_with(command, index, builtins[j]);
        }
    }

    char** execs = find_executable_completions(command);

    // No matches or multiple matches and is first tab press
    if (execs[0] == NULL || execs[1] != NULL && !is_double) {
        printf("%c", 7);  // Ring a bell
        return;
    }

    // Exactly 1 match
    if (execs[1] == NULL) {
        return complete_with(command, index, execs[0]);
    }

    // More matches and is second tab press
    printf("%c", 7);  // Ring a bell
    printf("\n%s", execs[0]);
    for (int i = 1; execs[i] != NULL; i++) {
        printf("  %s", execs[i]);
    }
    printf("\n$ %s", command);
}

char* read_input() {
    char* command = malloc(1024);
    int i = 0;
    char c, prev_c;
    while (1) {
        read(0, &c, 1);
        if (c == 127) {  // Backspace
            if (i == 0) continue;
            command[--i] = '\0';
            printf("\b \b");
        } else if (c == '\t') {
            handle_tab(command, &i, prev_c == '\t');
        } else if (c == '\n') {
            command[i] = '\0';
            printf("%c", c);
            break;
        } else {
            command[i++] = c;
            printf("%c", c);
        }
        prev_c = c;
    }
    return command;
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
    enable_raw_mode();

    while (1) {
        printf("$ ");
        char* command = read_input();

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

        if (strcmp(argv[0], "") == 0) continue;
        if (strcmp(argv[0], "exit") == 0) {
            disable_raw_mode();
            break;
        }

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
