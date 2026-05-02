#ifndef TYPES_H
#define TYPES_H

#include <sys/types.h>

typedef enum { NORMAL, IN_SINGLE_QUOTE, IN_DOUBLE_QUOTE } State;
typedef enum { NONE, STDOUT, STDERR, APPEND_STDOUT, APPEND_STDERR } RedirectMode;
typedef enum { RUNNING, STOPPED, DONE } JobStatus;

typedef struct {
    char** argv;

    char* stdout_path;
    char* stderr_path;
    int stdout_append;
    int stderr_append;
} Command;

typedef struct {
    pid_t pid;
    int job_number;
    JobStatus status;
    char* command;
} Job;

#endif
