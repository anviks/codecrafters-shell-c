#include "jobs.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

Job* jobs = NULL;
int job_count = 0;

void jobs_init() {
    jobs = malloc(1024 * sizeof(Job));
}

void jobs_add(pid_t pgid, char* command) {
    jobs[job_count] = (Job){
        .pid = pgid,
        .job_number = job_count + 1,
        .command = command,
        .status = RUNNING
    };
    printf("[%d] %d\n", jobs[job_count].job_number, jobs[job_count].pid);
    job_count++;
}

void jobs_print() {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == 0) continue;

        char marker = ' ';
        if (i == job_count - 2) marker = '-';
        else if (i == job_count - 1) marker = '+';

        char* status_text;
        switch (jobs[i].status) {
            case RUNNING: status_text = "Running"; break;
            case DONE: status_text = "Done"; break;
            case STOPPED: status_text = "Stopped"; break;
        }

        // Super hacky
        if (jobs[i].status == DONE) {
            jobs[i].command[strlen(jobs[i].command) - 1] = '\0';
        }

        printf("[%d]%c  %-23s %s\n", jobs[i].job_number, marker, status_text, jobs[i].command);
    }

    int i = 0;
    while (i < job_count) {
        if (jobs[i].status == DONE) {
            jobs[i] = (Job){0};
        }
        i++;
    }
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
