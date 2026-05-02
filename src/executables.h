#ifndef EXECUTABLES_H
#define EXECUTABLES_H

#include "types.h"

char* find_executable(char* name);
char** find_executable_completions(char* name);
void run_executable(Command command);

#endif
