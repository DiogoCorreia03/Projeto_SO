#include "../fs/operations.h"
#include "../utils/common.h"
#include "logging.h"
#include "string.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static volatile int running = TRUE;

void sigint_handler() { running = FALSE; }

int register_sub(int server_pipe, char *session_pipe_name, char *box) {
    void *message = calloc(REQUEST_LENGTH, sizeof(char));
    if (message == NULL) {
        WARN("Unable to alloc memory to register Subscriber.\n");
        return -1;
    }

    memcpy(message, SUB_REGISTER, sizeof(uint8_t));
    message += UINT8_T_SIZE;

    int pipe_n_bytes = strlen(session_pipe_name) > PIPE_NAME_LENGTH
                           ? PIPE_NAME_LENGTH
                           : strlen(session_pipe_name);
    memcpy(message, session_pipe_name, pipe_n_bytes);
    message += PIPE_NAME_LENGTH;

    int box_n_bytes =
        strlen(box) > BOX_NAME_LENGTH ? BOX_NAME_LENGTH : strlen(box);
    memcpy(message, box, box_n_bytes);

    message -= (UINT8_T_SIZE + PIPE_NAME_LENGTH);

    if (write(server_pipe, message, REQUEST_LENGTH) == -1) {
        WARN("Unable to write message.\n");
        free(message);
        return -1;
    }

    free(message);
    return 0;
}

int read_message(int session_pipe, char *buffer) {

    void *message = calloc(MESSAGE_SIZE, sizeof(char));
    if (message == NULL) {
        WARN("Unable to alloc memory to read message.\n");
        return -1;
    }

    if (read(session_pipe, message, MESSAGE_SIZE) <= 0) {
        free(message);
        return -1;
    }

    message += UINT8_T_SIZE;
    memcpy(buffer, message, BLOCK_SIZE);
    free(message);

    return 0;
}

int sub_destroy(int session_pipe, char *session_pipe_name, int server_pipe) {

    if (close(server_pipe) == -1) {
        WARN("End of Session: Failed to close the Server's Pipe.\n");
        return -1;
    }

    if (close(session_pipe) == -1) {
        WARN("End of Session: Failed to close the Session's Pipe.\n");
        return -1;
    }

    if (unlink(session_pipe_name) != 0 && errno != ENOENT) {
        WARN("End of session: Unlink(%s) failed: %s\n", session_pipe_name,
             strerror(errno));
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        WARN("Instead of 4 arguments, %d were passed.\n", argc);
        return -1;
    }

    char *server_pipe_name = argv[1];  // Server's Pipe name
    char *session_pipe_name = argv[2]; // Session's Pipe name
    char *box_name = argv[3];          // Box's name

    if (unlink(session_pipe_name) != 0 && errno != ENOENT) {
        WARN("Unlink(%s) failed: %s\n", session_pipe_name, strerror(errno));
        return -1;
    }

    if (mkfifo(session_pipe_name, 0777) != 0) {
        WARN("Unable to create Session's Pipe.\n");
        return -1;
    }

    int server_pipe = open(server_pipe_name, O_WRONLY);
    if (server_pipe == -1) {
        WARN("Unable to open Server's Pipe.\n");
        unlink(session_pipe_name);
        return -1;
    }

    if (register_sub(server_pipe, session_pipe_name, box_name) != 0) {
        WARN("Unable to register this Session in the Server.\n");
        close(server_pipe);
        unlink(session_pipe_name);
        return -1;
    }

    int session_pipe = open(session_pipe_name, O_RDONLY);
    if (session_pipe == -1) {
        WARN("Unable to open Session's Pipe.\n");
        close(server_pipe);
        unlink(session_pipe_name);
        return -1;
    }

    signal(SIGINT, sigint_handler);

    int message_counter = 0;
    char *buffer = calloc(BLOCK_SIZE, sizeof(char));
    if (buffer == NULL) {
        WARN("Unable to alloc memory to read message.\n");
        sub_destroy(session_pipe, session_pipe_name, server_pipe);
        return -1;
    }

    while (running) {
        if (read_message(session_pipe, buffer) != 0) {
            WARN("Error reading messages from box.\n");
            free(buffer);
            sub_destroy(session_pipe, session_pipe_name, server_pipe);
            return -1;
        }
        fprintf(stdout, "%s", buffer);
        message_counter++;
    }

    fprintf(stderr, "Messages sent: %d\n", message_counter);
    free(buffer);

    if (sub_destroy(session_pipe, session_pipe_name, server_pipe) != 0) {
        return -1;
    }

    return 0;
}
