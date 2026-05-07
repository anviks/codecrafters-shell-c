#include "jobs.h"
#include "array.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

Array jobs;

void init_jobs() {
    array_init(&jobs, sizeof(Job), NULL);
}

void add_job(pid_t pgid, char* command) {
    int job_number = jobs.count > 0
        ? ((Job*)array_at(&jobs, jobs.count - 1))->job_number + 1
        : 1;

    array_add(&jobs, &(Job){
        .pid = pgid,
        .job_number = job_number,
        .command = command,
        .status = RUNNING
    });

    printf("[%d] %d\n", job_number, pgid);
}

void delete_done_jobs() {
    int i = 0;
    while (i < jobs.count) {
        if (((Job*)array_at(&jobs, i))->status == DONE) {
            array_remove_at(&jobs, i);
        } else {
            i++;
        }
    }
}

void print_job(int job_index) {
    Job* job = array_at(&jobs, job_index);

    char marker = ' ';
    if (job_index == jobs.count - 2) marker = '-';
    else if (job_index == jobs.count - 1) marker = '+';

    char* status_text;
    switch (job->status) {
        case RUNNING: status_text = "Running"; break;
        case DONE: status_text = "Done"; break;
        case STOPPED: status_text = "Stopped"; break;
    }

    // Super hacky
    if (job->status == DONE) {
        job->command[strlen(job->command) - 1] = '\0';
    }

    printf("[%d]%c  %-23s %s\n", job->job_number, marker, status_text, job->command);
}

void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // find the job with this pid and mark it DONE
        for (int i = 0; i < jobs.count; i++) {
            Job* job = array_at(&jobs, i);
            if (job->pid == pid) {
                job->status = DONE;
                break;
            }
        }
    }
}
