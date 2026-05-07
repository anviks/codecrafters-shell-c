#ifndef EXECUTABLES_H
#define EXECUTABLES_H

#include "array.h"
#include "parser.h"

char* find_executable(char* name);
void find_executable_completions(Array* result, char* name);
void run_executable(Command command);

#endif
