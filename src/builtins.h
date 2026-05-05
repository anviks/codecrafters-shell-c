#ifndef BUILTINS_H
#define BUILTINS_H

#include "array.h"
#include "parser.h"

extern char* builtins[];
extern int history_entries_saved;
extern Array variables;

typedef struct {
    char* name;
    char* value;
} Variable;

int is_builtin(char* name);
void execute_builtin_command(Command command);

#endif
