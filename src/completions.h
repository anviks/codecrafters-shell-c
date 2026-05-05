#ifndef COMPLETIONS_H
#define COMPLETIONS_H

#include "array.h"

typedef struct {
    char* command;
    char* completer;
} Completion;

extern Array completions;
void init_completions();
char** shell_completion(const char* text, int start, int end);

#endif // COMPLETIONS_H
