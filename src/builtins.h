#ifndef BUILTINS_H
#define BUILTINS_H

#include "types.h"

extern char* builtins[];
extern int history_entries_saved;

int is_builtin(char* name);
void execute_builtin_command(Command command);

#endif
