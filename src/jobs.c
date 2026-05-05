#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

Job* jobs = NULL;
int job_count = 0;

void init_jobs() {
    jobs = malloc(1024 * sizeof(Job));
}

void add_job(pid_t pgid, char* command) {
    int job_number = job_count > 0
        ? jobs[job_count - 1].job_number + 1
        : 1;

    jobs[job_count] = (Job){
        .pid = pgid,
        .job_number = job_number,
        .command = command,
        .status = RUNNING
    };

    printf("[%d] %d\n", jobs[job_count].job_number, jobs[job_count].pid);

    job_count++;
}

void delete_done_jobs() {
    int i = 0;
    while (i < job_count) {
        if (jobs[i].status == DONE) {
            for (int j = i + 1; j < job_count; j++) {
                jobs[j - 1] = jobs[j];
            }
            job_count--;
        } else {
            i++;
        }
    }
}

void print_job(int job_index) {
    Job job = jobs[job_index];

    char marker = ' ';
    if (job_index == job_count - 2) marker = '-';
    else if (job_index == job_count - 1) marker = '+';

    char* status_text;
    switch (job.status) {
        case RUNNING: status_text = "Running"; break;
        case DONE: status_text = "Done"; break;
        case STOPPED: status_text = "Stopped"; break;
    }

    // Super hacky
    if (job.status == DONE) {
        job.command[strlen(job.command) - 1] = '\0';
    }

    printf("[%d]%c  %-23s %s\n", job.job_number, marker, status_text, job.command);
}

void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // find the job with this pid and mark it DONE
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].pid == pid) {
                jobs[i].status = DONE;
                break;
            }
        }
    }
}
