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

    // Function to register the Subscriber in the Server

    void *message = calloc(REQUEST_LENGTH, sizeof(char));
    if (message == NULL) {
        fprintf(stderr,"Unable to alloc memory to register Subscriber.\n");
        return -1;
    }

    // Registration code
    memcpy(message, &SUB_REGISTER, sizeof(uint8_t));
    message += UINT8_T_SIZE;

    // Subscriber's Pipe
    size_t pipe_n_bytes = strlen(session_pipe_name) > PIPE_NAME_LENGTH
                              ? PIPE_NAME_LENGTH
                              : strlen(session_pipe_name);
    memcpy(message, session_pipe_name, pipe_n_bytes);
    message += PIPE_NAME_LENGTH;

    // Box FIXME falta '/' para tfs_directory
    size_t box_n_bytes =
        strlen(box) > BOX_NAME_LENGTH ? BOX_NAME_LENGTH : strlen(box);
    memcpy(message, box, box_n_bytes);

    message -= (UINT8_T_SIZE + PIPE_NAME_LENGTH);

    if (write(server_pipe, message, REQUEST_LENGTH) == -1) {
        fprintf(stderr,"Unable to write message.\n");
        free(message);
        return -1;
    }

    free(message);
    return 0;
}

int read_message(int session_pipe, char *buffer) {

    // Function to read from a Pipe into a Buffer

    void *message = calloc(MESSAGE_SIZE + UINT8_T_SIZE, sizeof(char));
    if (message == NULL) {
        fprintf(stderr,"Unable to alloc memory to read message.\n");
        return -1;
    }

    if (read(session_pipe, message, MESSAGE_SIZE + UINT8_T_SIZE) <= 0) {
        free(message);
        return -1;
    }

    memcpy(buffer, message + UINT8_T_SIZE, MESSAGE_SIZE);
    free(message);

    return 0;
}

int sub_destroy(int session_pipe, char *session_pipe_name) {

    if (close(session_pipe) == -1) {
        fprintf(stderr,"End of Session: Failed to close the Session's Pipe.\n");
        return -1;
    }

    if (unlink(session_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stderr,"End of session: Unlink(%s) failed: %s\n", session_pipe_name,
             strerror(errno));
        return -1;
    }

    free(session_pipe_name);

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr,"Instead of 4 arguments, %d were passed.\n", argc);
        return -1;
    }

    // Server's Pipe name
    char *server_pipe_name = calloc(PIPE_NAME_LENGTH, sizeof(char));
    memcpy(server_pipe_name, PIPE_PATH, strlen(PIPE_PATH));
    memcpy(server_pipe_name + strlen(PIPE_PATH), argv[1],
           PIPE_NAME_LENGTH - strlen(PIPE_PATH));
    // Session's Pipe name
    char *session_pipe_name = calloc(PIPE_NAME_LENGTH, sizeof(char));
    memcpy(session_pipe_name, PIPE_PATH, strlen(PIPE_PATH));
    memcpy(session_pipe_name + strlen(PIPE_PATH), argv[2],
           PIPE_NAME_LENGTH - strlen(PIPE_PATH));
    // Box's name
    char *box_name = argv[3];

    if (unlink(session_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stderr,"Unlink(%s) failed: %s\n", session_pipe_name, strerror(errno));
        free(server_pipe_name);
        free(session_pipe_name);
        return -1;
    }

    if (mkfifo(session_pipe_name, 0777) != 0) {
        fprintf(stderr,"Unable to create Session's Pipe.\n");
        free(server_pipe_name);
        free(session_pipe_name);
        return -1;
    }

    int server_pipe = open(server_pipe_name, O_WRONLY);
    if (server_pipe == -1) {
        fprintf(stderr,"Unable to open Server's Pipe.\n");
        unlink(session_pipe_name);
        free(server_pipe_name);
        free(session_pipe_name);
        return -1;
    }

    if (register_sub(server_pipe, session_pipe_name, box_name) != 0) {
        fprintf(stderr,"Unable to register this Session in the Server.\n");
        close(server_pipe);
        unlink(session_pipe_name);
        free(server_pipe_name);
        free(session_pipe_name);
        return -1;
    }

    if (close(server_pipe) == -1) {
        unlink(session_pipe_name);
        free(server_pipe_name);
        free(session_pipe_name);
        return -1;
    }

    free(server_pipe_name);

    int session_pipe = open(session_pipe_name, O_RDONLY);
    if (session_pipe == -1) {
        fprintf(stderr,"Unable to open Session's Pipe.\n");
        unlink(session_pipe_name);
        free(session_pipe_name);
        return -1;
    }

    /*  Read messages from Session's Pipe and write them into Stdout.
     *  Messages are received one by one with '\0' at the end with a maximum
     *  size of 1024 bytes.
     */

    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        fprintf(stderr,"Unable to set signal handler.\n");
        sub_destroy(session_pipe, session_pipe_name);
        return -1;
    }

    int message_counter = 0;
    char *buffer = calloc(MESSAGE_SIZE, sizeof(char));
    if (buffer == NULL) {
        fprintf(stderr,"Unable to alloc memory to read message.\n");
        sub_destroy(session_pipe, session_pipe_name);
        return -1;
    }

    while (running) {
        if (read_message(session_pipe, buffer) != 0) {
            fprintf(stderr,"Error reading messages from box.\n");
            free(buffer);
            sub_destroy(session_pipe, session_pipe_name);
            return -1;
        }
        fprintf(stdout, "%s\n", buffer);
        message_counter++;
    }

    fprintf(stderr, "Messages sent: %d\n", message_counter);
    free(buffer);

    if (sub_destroy(session_pipe, session_pipe_name) != 0) {
        return -1;
    }

    return 0;
}
