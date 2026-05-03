#include "builtins.h"
#include "executables.h"
#include "types.h"
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

Completion* completions = NULL;
int completion_count = 0;
static char* current_completer = NULL;

static char* builtin_exec_generator(const char* text, int state) {
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
        while (exec_matches[exec_idx] != NULL) return strdup(exec_matches[exec_idx++]);
    }

    return NULL;
}

static char* script_completer_generator(const char* text, int state) {
    (void)text;
    static char** matches;
    static int idx;

    if (state == 0) {
        matches = NULL;
        idx = 0;

        setenv("COMP_LINE", rl_line_buffer, 1);
        char point_str[16];
        snprintf(point_str, sizeof(point_str), "%d", rl_point);
        setenv("COMP_POINT", point_str, 1);

        char command[256];
        sscanf(rl_line_buffer, "%255s", command);

        int pos = rl_point - strlen(text) - 1;
        while (pos > 0 && rl_line_buffer[pos] == ' ') pos--;
        int end = pos;
        while (pos > 0 && rl_line_buffer[pos - 1] != ' ') pos--;
        char prev[256];
        strncpy(prev, rl_line_buffer + pos, end - pos + 1);
        prev[end - pos + 1] = '\0';

        char script[2048];
        snprintf(script, sizeof(script), "%s %s %s %s", current_completer, command, text, prev);

        FILE* f = popen(script, "r");
        if (f) {
            matches = malloc(1024 * sizeof(char*));
            int i = 0;
            char line[1024];
            while (fgets(line, sizeof(line), f)) {
                line[strcspn(line, "\n")] = '\0';
                matches[i++] = strdup(line);
            }
            matches[i] = NULL;
            pclose(f);
        }
    }

    if (!matches) return NULL;
    while (matches[idx] != NULL) return matches[idx++];

    return NULL;
}

void init_completions() {
    completions = malloc(1024 * sizeof(Completion));
}

void add_completion(char* command, char* completer) {
    completions[completion_count++] = (Completion){
        .command = strdup(command),
        .completer = strdup(completer)
    };
}

void remove_completion(char* command) {
    for (int i = 0; i < completion_count; i++) {
        if (strcmp(completions[i].command, command) == 0) {
            for (int j = i + 1; j < completion_count; j++) {
                completions[j - 1] = completions[j];
            }
            completion_count--;
            break;
        }
    }    
}

Completion* find_completion(char* command) {
    for (int i = 0; i < completion_count; i++) {
        if (strcmp(completions[i].command, command) == 0) {
            return &completions[i];
        }
    }

    return NULL;
}

char** shell_completion(const char* text, int start, int end) {
    (void)end;
    if (start == 0) return rl_completion_matches(text, builtin_exec_generator);

    char cmd[256];
    sscanf(rl_line_buffer, "%255s", cmd);
    Completion* completion = find_completion(cmd);
    if (!completion) return NULL;

    current_completer = completion->completer;

    return rl_completion_matches(text, script_completer_generator);
}
