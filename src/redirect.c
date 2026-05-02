#include "types.h"
#include <stdio.h>
#include <unistd.h>

void apply_redirects(Command* cmd) {
    if (cmd->stdout_path != NULL) {
        freopen(cmd->stdout_path, cmd->stdout_append ? "a" : "w", stdout);
    }

    if (cmd->stderr_path != NULL) {
        freopen(cmd->stderr_path, cmd->stderr_append ? "a" : "w", stderr);
    }
}

void restore_redirects(Command* cmd) {
    if (cmd->stdout_path != NULL) {
        freopen("/dev/tty", "w", stdout);
        setbuf(stdout, NULL);
    } else if (cmd->stderr_path != NULL) {
        freopen("/dev/tty", "w", stderr);
    }
}
