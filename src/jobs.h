#ifndef JOBS_H
#define JOBS_H

#include "array.h"
#include <sys/types.h>

typedef enum { RUNNING, STOPPED, DONE } JobStatus;
typedef struct {
    pid_t pid;
    int job_number;
    JobStatus status;
    char* command;
} Job;

extern Array jobs;

void init_jobs();
void add_job(pid_t pgid, char* command);
void print_job(int job_index);
void delete_done_jobs();
void sigchld_handler(int sig);

#endif
