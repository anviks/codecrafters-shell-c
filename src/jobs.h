#ifndef JOBS_H
#define JOBS_H

#include "types.h"

extern Job* jobs;
extern int job_count;

void jobs_init();
void jobs_add(pid_t pgid, char* command);
void jobs_print();
void sigchld_handler(int sig);

#endif
