#include "parser.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#define PATH_LIST_SEPARATOR ";"
#else
#define PATH_LIST_SEPARATOR ":"
#endif

static int cmp(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

char** find_executable_completions(char* name) {
    int capacity = 1024;
    char** result = malloc(capacity * sizeof(char*));
    int i = 0;
    char* path_env = strdup(getenv("PATH"));
    char *path, *path_state;
    path = strtok_r(path_env, PATH_LIST_SEPARATOR, &path_state);
    while (path != NULL) {
        DIR* d = opendir(path);
        struct dirent* dir;
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                if (strncmp(dir->d_name, name, strlen(name)) != 0) continue;

                char* fullpath = malloc(strlen(path) + strlen(dir->d_name) + 2);
                snprintf(fullpath, strlen(path) + strlen(dir->d_name) + 2, "%s/%s", path, dir->d_name);

                if (access(fullpath, X_OK) != 0) {
                    free(fullpath);
                    continue;
                }
                free(fullpath);
                if (i + 1 >= capacity) {
                    capacity *= 2;
                    result = realloc(result, capacity * sizeof(char*));
                }
                result[i++] = strdup(dir->d_name);
            }
            closedir(d);
        }
        path = strtok_r(NULL, PATH_LIST_SEPARATOR, &path_state);
    }
    free(path_env);
    qsort(result, i, sizeof(char*), cmp);
    result[i] = NULL;

    return result;
}

char* find_executable(char* name) {
    char* path_env = strdup(getenv("PATH"));
    char *path, *path_state;
    path = strtok_r(path_env, PATH_LIST_SEPARATOR, &path_state);
    while (path != NULL) {
        DIR* d = opendir(path);
        struct dirent* dir;
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                if (strcmp(dir->d_name, name) != 0) continue;

                char* fullpath = malloc(strlen(path) + strlen(dir->d_name) + 2);
                snprintf(fullpath, strlen(path) + strlen(dir->d_name) + 2, "%s/%s", path, dir->d_name);

                if (access(fullpath, X_OK) != 0) {
                    free(fullpath);
                    continue;
                }

                closedir(d);
                free(path_env);
                return fullpath;
            }
            closedir(d);
        }
        path = strtok_r(NULL, PATH_LIST_SEPARATOR, &path_state);
    }
    free(path_env);
    return NULL;
}

void run_executable(Command command) {
    char* exec_path = find_executable(command.argv[0]);
    if (!exec_path) {
        fprintf(stderr, "%s: command not found\n", command.argv[0]);
        exit(127);
    }

    execv(exec_path, command.argv);
    exit(127);
}
