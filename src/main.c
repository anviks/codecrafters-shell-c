#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <readline/history.h>
#include <readline/readline.h>
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
        if (strncmp(b, text, strlen(text)) == 0) return strdup(b);
    }

    if (exec_matches) {
        while (exec_matches[exec_idx] != NULL) return exec_matches[exec_idx++];
    }

    return NULL;
}

static char** shell_completion(const char* text, int start, int end) {
    (void)end;
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
        fprintf(stderr, "%s: not found\n", argv[1]);
}

typedef enum { NORMAL, IN_SINGLE_QUOTE, IN_DOUBLE_QUOTE } State;
typedef enum { NONE, STDOUT, STDERR, APPEND_STDOUT, APPEND_STDERR } RedirectMode;

typedef struct {
    char** argv;
    int argc;

    char* stdout_path;
    char* stderr_path;
    int stdout_append;
    int stderr_append;
} Command;

void apply_redirect(Command* cmd, RedirectMode mode, const char* path) {
    switch (mode) {
        case APPEND_STDOUT:
            cmd->stdout_append = 1;
            // fallthrough
        case STDOUT:
            cmd->stdout_path = strdup(path);
            break;
        case APPEND_STDERR:
            cmd->stderr_append = 1;
            // fallthrough
        case STDERR:
            cmd->stderr_path = strdup(path);
            break;
        default:
            break;
    }
}

void push_command(Command* commands, int* count, Command* cur, int* argv_i) {
    cur->argc = *argv_i;
    commands[(*count)++] = *cur;
}

int parse_commands(char* input, Command* commands) {
    RedirectMode redirect_mode = NONE;
    State state = NORMAL;
    Command cur_cmd = {0};
    cur_cmd.argv = malloc(1024 * sizeof(char*));
    char* cur_arg = malloc(1024);
    int cur_cmd_i = 0, cur_argv_i = 0, cur_arg_i = 0;

    for (int i = 0; input[i] != '\0'; i++) {
        char c = input[i];
        switch (state) {
        case NORMAL:
            if (c == '\\')
                cur_arg[cur_arg_i++] = input[++i];
            else if (c == '\'')
                state = IN_SINGLE_QUOTE;
            else if (c == '"')
                state = IN_DOUBLE_QUOTE;
            else if (c == ' ') {
                if (cur_arg_i > 0) {
                    cur_arg[cur_arg_i] = '\0';
                    if (strcmp(cur_arg, ">") == 0 || strcmp(cur_arg, "1>") == 0) {
                        redirect_mode = STDOUT;
                    } else if (strcmp(cur_arg, "2>") == 0) {
                        redirect_mode = STDERR;
                    } else if (strcmp(cur_arg, ">>") == 0 || strcmp(cur_arg, "1>>") == 0) {
                        redirect_mode = APPEND_STDOUT;
                    } else if (strcmp(cur_arg, "2>>") == 0) {
                        redirect_mode = APPEND_STDERR;
                    } else if (strcmp(cur_arg, "|") == 0) {
                        push_command(commands, &cur_cmd_i, &cur_cmd, &cur_argv_i);
                        cur_cmd = (Command){0};
                        cur_cmd.argv = malloc(1024 * sizeof(char*));
                        cur_argv_i = 0;
                        free(cur_arg);
                        cur_arg = malloc(1024);
                        cur_arg_i = 0;
                    } else if (redirect_mode == NONE) {
                        cur_cmd.argv[cur_argv_i++] = strdup(cur_arg);
                    } else {
                        apply_redirect(&cur_cmd, redirect_mode, cur_arg);
                        redirect_mode = NONE;
                    }
                    cur_arg_i = 0;
                }
            } else
                cur_arg[cur_arg_i++] = c;
            break;
        case IN_SINGLE_QUOTE:
            if (c == '\'')
                state = NORMAL;
            else
                cur_arg[cur_arg_i++] = c;
            break;
        case IN_DOUBLE_QUOTE:
            if (c == '\\')
                cur_arg[cur_arg_i++] = input[++i];
            else if (c == '"')
                state = NORMAL;
            else
                cur_arg[cur_arg_i++] = c;
            break;
        }
    }

    cur_arg[cur_arg_i] = '\0';
    if (redirect_mode == NONE) {
        cur_cmd.argv[cur_argv_i++] = strdup(cur_arg);
    } else {
        apply_redirect(&cur_cmd, redirect_mode, cur_arg);
        redirect_mode = NONE;
    }

    free(cur_arg);

    push_command(commands, &cur_cmd_i, &cur_cmd, &cur_argv_i);

    return cur_cmd_i;
}

void execute_command(Command command) {
    if (command.stdout_path != NULL) {
        freopen(command.stdout_path, command.stdout_append ? "a" : "w", stdout);
    }

    if (command.stderr_path != NULL) {
        freopen(command.stderr_path, command.stderr_append ? "a" : "w", stderr);
    }

    if (strcmp(command.argv[0], "echo") == 0) {
        printf("%s", command.argv[1]);
        for (int i = 2; i < command.argc; i++) {
            printf(" %s", command.argv[i]);
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
        if (command.argc == 1) {
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
    } else {
        char* exec_path = find_executable(command.argv[0]);
        if (!exec_path) {
            fprintf(stderr, "%s: command not found\n", command.argv[0]);
            return;
        }

        pid_t pid = fork();

        if (pid == 0) {
            execv(exec_path, command.argv);
            exit(127);
        } else {
            waitpid(pid, 0, 0);
        }

        free(exec_path);
    }

    if (command.stdout_path != NULL) {
        freopen("/dev/tty", "w", stdout);
        setbuf(stdout, NULL);  // Flush after every printf
    } else if (command.stderr_path != NULL) {
        freopen("/dev/tty", "w", stderr);
    }
}

void log_args(int command_count, Command* commands) {
    for (int i = 0; i < command_count; i++) {
        for (int j = 0; j < commands[i].argc; j++) {
            printf("Arg %d: %s\n", j, commands[i].argv[j]);
        }
        printf("\n");
    }
}

int main() {
    setbuf(stdout, NULL);  // Flush after every printf
    rl_attempted_completion_function = shell_completion;

    char* input;
    while ((input = readline("$ ")) != NULL) {
        if (*input) add_history(input);

        Command* commands = malloc(128 * sizeof(Command));
        int command_count = parse_commands(input, commands);

        free(input);

        if (commands[0].argc == 0 || strcmp(commands[0].argv[0], "") == 0) continue;
        if (strcmp(commands[0].argv[0], "exit") == 0) break;
        execute_command(commands[0]);

        for (int i = 0; i < command_count; i++) {
            for (int j = 0; j < commands[i].argc; j++) {
                free(commands[i].argv[j]);
            }

            free(commands[i].argv);

            free(commands[i].stdout_path);
            free(commands[i].stderr_path);
        }

        free(commands);
    }

    return 0;
}
