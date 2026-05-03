#include "types.h"
#include <stdlib.h>
#include <string.h>

Completion* completions = NULL;
int completion_count = 0;

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
