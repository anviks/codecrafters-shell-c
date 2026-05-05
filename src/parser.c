#include "parser.h"
#include <stdlib.h>
#include <string.h>

typedef enum { NORMAL, IN_SINGLE_QUOTE, IN_DOUBLE_QUOTE } State;
typedef enum { NONE, STDOUT, STDERR, APPEND_STDOUT, APPEND_STDERR } RedirectMode;

static Command new_command() {
    Command command = {0};
    command.argv = malloc(1024 * sizeof(char*));
    return command;
}

static void push_command(Command* commands, int* count, Command* cur, int argv_i) {
    cur->argv[argv_i] = NULL;
    commands[(*count)++] = *cur;
}

static void apply_redirect(Command* cmd, RedirectMode mode, const char* path) {
    switch (mode) {
        case APPEND_STDOUT:
            cmd->stdout_append = 1;
            // fallthrough
        case STDOUT: cmd->stdout_path = strdup(path); break;
        case APPEND_STDERR:
            cmd->stderr_append = 1;
            // fallthrough
        case STDERR: cmd->stderr_path = strdup(path); break;
        default: break;
    }
}

int parse_commands(char* input, Command* commands) {
    RedirectMode redirect_mode = NONE;
    State state = NORMAL;
    Command cur_cmd = new_command();
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
                        push_command(commands, &cur_cmd_i, &cur_cmd, cur_argv_i);
                        cur_cmd = new_command();
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

    if (cur_arg_i > 0) {
        cur_arg[cur_arg_i] = '\0';
        if (redirect_mode == NONE) {
            cur_cmd.argv[cur_argv_i++] = strdup(cur_arg);
        } else {
            apply_redirect(&cur_cmd, redirect_mode, cur_arg);
            redirect_mode = NONE;
        }
    }

    free(cur_arg);

    push_command(commands, &cur_cmd_i, &cur_cmd, cur_argv_i);

    return cur_cmd_i;
}
