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

void init_completions() {
    completions = malloc(1024 * sizeof(Completion));
}

void add_completion(char* command, char* completer) {
    completions[completion_count++] = (Completion){
        .command = strdup(command),
        .completer = strdup(completer)
    };
}

Completion* find_completion(char* command) {
    for (int i = 0; i < completion_count; i++) {
        if (strcmp(completions[i].command, command) == 0) {
            return &completions[i];
        }
    }

    return NULL;
}

static char* completion_generator(const char* text, int state) {
    static int builtin_idx;
    static char** exec_matches;
    static int exec_idx;
    static char** completer_matches;
    static int completer_idx;

    if (state == 0) {
        builtin_idx = 0;
        exec_matches = find_executable_completions((char*)text);
        exec_idx = 0;
        completer_matches = NULL;
        completer_idx = 0;

        if (current_completer != NULL) {
            FILE* f = popen(current_completer, "r");
            if (f) {
                completer_matches = malloc(1024 * sizeof(char*));
                int i = 0;
                char line[1024];
                while (fgets(line, sizeof(line), f)) {
                    line[strcspn(line, "\n")] = '\0';
                    completer_matches[i++] = strdup(line);
                }
                completer_matches[i] = NULL;
                pclose(f);
            }
        }
    }

    if (current_completer != NULL) {
        if (!completer_matches) return NULL;

        while (completer_matches[completer_idx] != NULL) {
            return completer_matches[completer_idx++];
        }

        return NULL;
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

char** shell_completion(const char* text, int start, int end) {
    (void)end;
    current_completer = NULL;
    if (start == 0) return rl_completion_matches(text, completion_generator);

    char cmd[256];
    sscanf(rl_line_buffer, "%255s", cmd);
    Completion* completion = find_completion(cmd);
    if (!completion) return NULL;

    current_completer = completion->completer;

    return rl_completion_matches(text, completion_generator);
}
