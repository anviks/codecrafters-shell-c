#include "types.h"
#include "executables.h"
#include "builtins.h"
#include "parser.h"
#include "redirect.h"
#include <readline/history.h>
#include <readline/readline.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

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

void log_args(int command_count, Command* commands) {
    for (int i = 0; i < command_count; i++) {
        for (int j = 0; commands[i].argv[j] != NULL; j++) {
            printf("Arg %d: %s\n", j, commands[i].argv[j]);
        }
        printf("\n");
    }
}

int main() {
    setbuf(stdout, NULL);  // Flush after every printf
    rl_attempted_completion_function = shell_completion;

    char* histfile = getenv("HISTFILE");
    if (histfile != NULL && strcmp(histfile, "") != 0) {
        read_history(histfile);
        history_entries_saved = history_length;
    }

    Job* jobs = malloc(1024 * sizeof(Job));
    int job_i = 0;

    char* input;
    while ((input = readline("$ ")) != NULL) {
        Command* commands = malloc(128 * sizeof(Command));
        int command_count = parse_commands(input, commands);

        if (
            commands[0].argv[0] != NULL
            && (history_length == 0
                || strcmp(input, history_get(history_base + history_length - 1)->line) != 0)
        ) {
            add_history(input);
        }

        if (commands[0].argv[0] == NULL) continue;

        int is_job = 0;
        char** last_args = commands[command_count - 1].argv;
        int last_argv = -1;
        while (last_args[++last_argv] != NULL);
        last_argv--;

        if (strcmp(last_args[last_argv], "&") == 0) {
            last_args[last_argv] = NULL;
            jobs[job_i] = (Job){
                .command = strdup(input),
                .job_number = job_i + 1,
                .status = RUNNING,
            };
            job_i++;
            is_job = 1;
        }

        free(input);

        if (strcmp(commands[0].argv[0], "exit") == 0) break;

        if (command_count == 1 && !is_job) {
            Command command = commands[0];
            apply_redirects(&command);

            if (is_builtin(command.argv[0])) {
                execute_builtin_command(command);
            } else {
                pid_t pid = fork();

                if (pid == 0) {
                    run_executable(command);
                    exit(0);
                } else {
                    waitpid(pid, 0, 0);
                }
            }

            restore_redirects(&command);
        } else {
            int pipes[command_count - 1][2];

            for (int i = 0; i < command_count - 1; i++) {
                pipe(pipes[i]);
            }

            pid_t pgid = 0;

            for (int i = 0; i < command_count; i++) {
                pid_t pid = fork();

                if (pid == 0) {
                    setpgid(0, pgid);

                    // read from left
                    if (i > 0) {
                        dup2(pipes[i - 1][0], STDIN_FILENO);
                    }

                    // write to right
                    if (i < command_count - 1) {
                        dup2(pipes[i][1], STDOUT_FILENO);
                    }

                    // close all other fd-s, since they're not needed
                    for (int j = 0; j < command_count - 1; j++) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }

                    Command command = commands[i];
                    apply_redirects(&command);

                    if (is_builtin(command.argv[0])) {
                        execute_builtin_command(command);
                    } else {
                        run_executable(command);
                    }

                    exit(0);
                }
                if (pgid == 0) pgid = pid;
                setpgid(pid, pgid);
            }

            if (is_job) {
                jobs[job_i - 1].pid = pgid;
                printf("[%d] %d\n", jobs[job_i - 1].job_number, jobs[job_i - 1].pid);
            }

            // now close pipe fd-s in the parent process
            for (int i = 0; i < command_count - 1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }

            if (is_job) {
                waitpid(-pgid, NULL, WNOHANG);
            } else {
                while (waitpid(-pgid, NULL, 0) > 0);
            }
        }

        for (int i = 0; i < command_count; i++) {
            for (int j = 0; commands[i].argv[j] != NULL; j++) {
                free(commands[i].argv[j]);
            }

            free(commands[i].argv);

            free(commands[i].stdout_path);
            free(commands[i].stderr_path);
        }

        free(commands);
    }

    if (histfile != NULL && strcmp(histfile, "") != 0) {
        append_history(history_length - history_entries_saved, histfile);
    }

    return 0;
}
