#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
#define PATH_LIST_SEPARATOR ";"
#else
#define PATH_LIST_SEPARATOR ":"
#endif

char* find_executable(char* name) {
    char* path_env = strdup(getenv("PATH"));
    char *path, *path_state;
    path = strtok_r(path_env, PATH_LIST_SEPARATOR, &path_state);
    while (path != NULL) {
        DIR* d = opendir(path);
        struct dirent* dir;
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                char* fullpath = malloc(strlen(path) + strlen(name) + 2);
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);

                if (strcmp(dir->d_name, name) == 0 && access(fullpath, X_OK) == 0) {
                    closedir(d);
                    free(path_env);
                    return fullpath;
                }
            }
            closedir(d);
        }
        path = strtok_r(NULL, PATH_LIST_SEPARATOR, &path_state);
    }
    free(path_env);
    return NULL;
}

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
                char* exec_path = find_executable(args);
                if (exec_path)
                    printf("%s\n", exec_path);
                else
                    printf("%s: not found\n", args);
            }
        } else {
            char *arg, *arg_state;
            arg = strtok_r(strdup(command), " ", &arg_state);
            char* exec_path = find_executable(arg);
            if (!exec_path) {
                printf("%s: command not found\n", command);
                continue;
            }

            pid_t pid = fork();

            if (pid == 0) {
                char** argv = malloc(1024);
                int i = 0;
                arg = strtok_r(NULL, " ", &arg_state);
                while (arg != NULL) {
                    argv[i++] = arg;
                    arg = strtok_r(NULL, " ", &arg_state);
                }
                argv[i] = NULL;
                execv(exec_path, argv);
            }
        }
    }

    return 0;
}
