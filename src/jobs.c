#include "jobs.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>

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
        char marker = ' ';
        if (i == job_count - 2) marker = '-';
        else if (i == job_count - 1) marker = '+';

        char* status_text;
        switch (jobs[i].status) {
            case RUNNING: status_text = "Running"; break;
            case DONE: status_text = "Done"; break;
            case STOPPED: status_text = "Stopped"; break;
        }

        printf("[%d]%c  %-23s %s\n", jobs[i].job_number, marker, status_text, jobs[i].command);
    }
}
