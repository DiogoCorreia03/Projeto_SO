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
    void *message = calloc(REGISTER_LENGTH, sizeof(char));

    memcpy(message, PUB_REGISTER, sizeof(uint8_t));
    message += sizeof(uint8_t);

    int pipe_n_bytes = strlen(session_pipe_name) > PIPE_NAME_LENGTH
                           ? PIPE_NAME_LENGTH
                           : strlen(session_pipe_name);
    memcpy(message, session_pipe_name, pipe_n_bytes);
    message += pipe_n_bytes;

    int box_n_bytes =
        strlen(box) > BOX_NAME_LENGTH ? BOX_NAME_LENGTH : strlen(box);
    memcpy(message, box, box_n_bytes);

    if (write(server_pipe, message, REGISTER_LENGTH) == -1) {
        WARN("Unnable to write message.\n");
        free(message);
        return -1;
    }

    free(message);
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

    // Server's Pipe
    int server_pipe = open(server_pipe_name, O_WRONLY);
    if (server_pipe == -1) {
        WARN("Unable to open Server's Pipe.\n");
        exit(EXIT_FAILURE); // FIXME ou return -1; ?
    }

    if (unlink(session_pipe_name) != 0 && errno != ENOENT) {
        WARN("Unlink(%s) failed: %s\n", session_pipe_name, strerror(errno));
        return -1;
    }

    if (mkfifo(session_pipe_name, 0777) != 0) {
        WARN("Unnable to create Session's Pipe.\n");
        return -1;
    }

    // Request to register the Publisher in the Server
    if (register_pub(server_pipe, session_pipe_name, box_name) != 0) {
        WARN("Unnable to register Session's Pipe in the Server.\n");
        return -1;
    }

    int session_pipe = open(session_pipe_name, O_WRONLY);
    if (session_pipe == -1) {
        WARN("Unnable to open Session's Pipe.\n");
        return -1;
    }

    char c = 'a';
    int i = 0;
    size_t max_size = file_size();
    char buffer[max_size];
    memset(buffer, 0, max_size);

    while (c = (char)getchar() != EOF) {
        if (i < max_size - 1) {
            if (c == '\n') {
                c = '\0';
                i = max_size - 1;
            }
            buffer[i] = c;
        }

        if (i >= max_size - 1) {
            if (write(session_pipe, buffer, max_size) == -1) {
                WARN("Unnable to write message.\n");
                return -1;
            }
            // write message sent e mudar o write para funcao á parte para o por o codigo
            i = 0;
            memset(buffer, 0, max_size);
        }

        i++;
    }

    // Buffer has an written message (CTRL-D was pressed)
    if (strlen(buffer) > 0) {
        if (write(session_pipe, buffer, max_size) == -1) {
            WARN("Unnable to write message.\n");
            return -1;
        }
        // write message sent e mudar o write para funcao á parte para o por o codigo
    }

    if (close(session_pipe) == -1) {
        WARN("End of Session: Failed to close the Session's Pipe.\n");
        return -1;
    }

    if (unlink(session_pipe_name) != 0 && errno != ENOENT) {
        WARN("End of session unlink(%s) failed: %s\n", session_pipe_name,
             strerror(errno));
        return -1;
    }

    return 0;
}
