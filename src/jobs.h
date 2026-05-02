#ifndef JOBS_H
#define JOBS_H

#include "types.h"

extern Job* jobs;
extern int job_count;

void init_jobs();
void add_job(pid_t pgid, char* command);
void print_job(int job_index);
void delete_done_jobs();
void sigchld_handler(int sig);

#endif
