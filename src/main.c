#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    // Flush after every printf
    setbuf(stdout, NULL);

    while (1) {
        printf("$ ");
        char command[1024];
        fgets(command, sizeof(command), stdin);
        command[strlen(command) - 1] = '\0';

        if (strcmp(command, "") == 0) continue;
        if (strcmp(command, "exit") == 0) break;

        if (strncmp(command, "echo ", 5) == 0) {
            printf("%s\n", command + 5);
        } else if (strncmp(command, "type ", 5) == 0) {
            char* args = command + 5;
            if (strcmp(args, "echo") == 0 || strcmp(args, "exit") == 0 || strcmp(args, "type") == 0) {
                printf("%s is a shell builtin\n", args);
            } else {
                printf("%s: not found\n", args);
            }
        } else {
            printf("%s: command not found\n", command);
        }
    }

    return 0;
}
