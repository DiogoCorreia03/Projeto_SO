#include "../fs/operations.h"
#include "logging.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void main_thread(char *server_pipe_name) {

    int server_pipe = open(server_pipe_name, O_RDONLY);
    if (server_pipe == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void working_thread() {}

int main(int argc, char **argv) {

    if (argc != 3) {
        WARN("Instead of 3 arguments, %d were passed.\n", argc);
        return -1;
    }

    // veririficar q argv[1] e argv[2] são validos

    // Statrt TFS
    if (tfs_init(NULL) != 0) {
        WARN("Unnable to start TFS.\n");
        return -1;
    }

    // Server's Pipe
    char *server_pipe_name = argv[1];

    char *c = '\0';
    long max_sessions = strtol(argv[2], &c, 10);
    if (errno != 0 || *c != '\0' || max_sessions > INT_MAX ||
        max_sessions < INT_MIN) {
        WARN("Invalid Max Sessions value.\n");
        tfs_destroy(); // FIXME
        return -1;
    }

    if (unlink(server_pipe_name) != 0 && errno != ENOENT) {
        WARN("Unlink(%s) failed: %s\n", server_pipe_name, strerror(errno));
        // FIXME é preciso dar tfs_destroy, close etc?
        return -1;
    }

    if (mkfifo(server_pipe_name, 0777) != 0) {
        WARN("Unnable to create Server's Pipe.\n");
        return -1;
    }

    // Create Working Threads for each Session
    pthread_t sessions_tid[max_sessions];
    for (int i = 0; i < max_sessions; i++) {
        if (pthread_create(&sessions_tid[i], NULL, working_thread, NULL) != 0) {
            WARN("Error creating Thread(%d)\n", i);
            return -1;
        }
    }

    int server_pipe = open(server_pipe_name, O_RDONLY); // FIXME

    if (close(server_pipe) == -1) {
        WARN("Error closing Server's Pipe.\n");
        return -1;
    }

    if (unlink(server_pipe_name) == -1) {
        WARN("Unlink(%s) failed: %s\n", server_pipe_name, strerror(errno));
        return -1;
    }

    if (tfs_destroy() != 0) {
        WARN("Error destroying TFS.\n");
        return -1;
    }

    return 0;
}
