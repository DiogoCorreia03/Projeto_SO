#include "../fs/operations.h"
#include "../utils/common.h"
#include "logging.h"
#include "string.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int register_pub(int server_pipe, char *session_pipe_name, char *box) {
    void *message = calloc(REQUEST_LENGTH, sizeof(char));
    if (message == NULL) {
        WARN("Unnable to alloc memory to register Publisher.\n");
        return -1;
    }

    memcpy(message, PUB_REGISTER, sizeof(uint8_t));
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
        WARN("Unnable to write message.\n");
        free(message);
        return -1;
    }

    free(message);
    return 0;
}

int send_message(int session_pipe, char message) {
    void *to_send = calloc(MESSAGE_SIZE, sizeof(char));
    if (to_send == NULL) {
        WARN("Unnable to alloc memory to send message.\n");
        return -1;
    }

    memcpy(to_send, PUB_2_SERVER, UINT8_T_SIZE);
    to_send += UINT8_T_SIZE;

    memcpy(to_send, message, strlen(message));
    to_send -= UINT8_T_SIZE;

    ssize_t bytes_written = write(session_pipe, to_send, MESSAGE_SIZE);

    free(to_send);

    return bytes_written;
}

int pub_destroy(int session_pipe, char *session_pipe_name, int server_pipe) {

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

    // Session's Pipe
    if (unlink(session_pipe_name) != 0 && errno != ENOENT) {
        WARN("Unlink(%s) failed: %s\n", session_pipe_name, strerror(errno));
        return -1;
    }

    if (mkfifo(session_pipe_name, 0777) != 0) {
        WARN("Unnable to create Session's Pipe.\n");
        return -1;
    }

    // Server's Pipe
    int server_pipe = open(server_pipe_name, O_WRONLY);
    if (server_pipe == -1) {
        WARN("Unable to open Server's Pipe.\n");
        unlink(session_pipe_name);
        return -1;
    }

    // Request to register the Publisher in the Server
    if (register_pub(server_pipe, session_pipe_name, box_name) != 0) {
        WARN("Unnable to register this Session in the Server.\n");
        close(server_pipe);
        unlink(session_pipe_name);
        return -1;
    }

    int session_pipe = open(session_pipe_name, O_WRONLY);
    if (session_pipe == -1) {
        WARN("Unnable to open Session's Pipe.\n");
        close(server_pipe);
        unlink(session_pipe_name);
        return -1;
    }

    char c = 'a';
    int i = 0;
    char buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);

    while (c = (char)getchar() != EOF) {
        if (i < BLOCK_SIZE - 1) {
            if (c == '\n') {
                c = '\0';
                i = BLOCK_SIZE - 1;
            }
            buffer[i] = c;
        }

        if (i >= BLOCK_SIZE - 1) {
            if (send_message(session_pipe, buffer) < 0) {
                WARN("Unnable to write message.\n");
                pub_destroy(session_pipe, session_pipe_name, server_pipe);
                return -1;
            }
            i = 0;
            memset(buffer, 0, BLOCK_SIZE);
        }

        i++;
    }

    if (pub_destroy(session_pipe, session_pipe_name, server_pipe) != 0) {
        return -1;
    }

    return 0;
}

// FIXME
/*// Buffer has an written message (CTRL-D was pressed)
    if (strlen(buffer) > 0) {
        if (write(session_pipe, buffer, BLOCK_SIZE) <= 0) {
            WARN("Unnable to write message.\n");
            return -1;
        }
        // write message sent e mudar o write para funcao á parte para o por o
        // codigo
    }*/
