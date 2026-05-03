#include "types.h"
#include "executables.h"
#include "builtins.h"
#include "parser.h"
#include "redirect.h"
#include "jobs.h"
#include "completions.h"
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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
    init_jobs();
    init_completions();
    signal(SIGCHLD, sigchld_handler);

    char* histfile = getenv("HISTFILE");
    if (histfile != NULL && strcmp(histfile, "") != 0) {
        read_history(histfile);
        history_entries_saved = history_length;
    }

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
            is_job = 1;
        }

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
                add_job(pgid, strdup(input));
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

        for (int i = 0; i < job_count; i++) {
            if (jobs[i].status == DONE) {
                print_job(i);
            }
        }
        delete_done_jobs();

        free(input);

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
