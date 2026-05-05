#ifndef PARSER_H
#define PARSER_H

typedef struct {
    char** argv;

    char* stdout_path;
    char* stderr_path;
    int stdout_append;
    int stderr_append;
} Command;

int parse_commands(char* input, Command* commands);

#endif
