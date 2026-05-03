#ifndef COMPLETIONS_H
#define COMPLETIONS_H

#include "types.h"

extern Completion* completions;
extern int completion_count;
void init_completions();
void add_completion(char* command, char* completer);
Completion* find_completion(char* command);

#endif // COMPLETIONS_H
