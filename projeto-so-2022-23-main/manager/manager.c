#include "../fs/operations.h"
#include "../utils/common.h"
#include "logging.h"
#include "state.h"
#include "string.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int box_request(char *server_pipe, char *session_pipe_name, char *box,
                uint8_t code) {

    // Send the request to the Server

    void *message = calloc(REQUEST_LENGTH, sizeof(char));
    if (message == NULL) {
        WARN("Unable to alloc memory to request box action.\n");
        return -1;
    }

    memcpy(message, code, sizeof(uint8_t));
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

    // Read the answer from the Server

    int session_pipe = open(session_pipe_name, O_RDONLY);
    if (session_pipe == -1) {
        WARN("Unable to open Session's Pipe.\n");
        return -1;
    }

    char buffer[TOTAL_RESPONSE_LENGTH];
    memset(buffer, 0, TOTAL_RESPONSE_LENGTH);
    if (read(session_pipe, buffer, TOTAL_RESPONSE_LENGTH) == -1) {
        WARN("Unable to read Server's answer to request.\n");
    }

    int32_t return_code;
    memcpy(return_code, buffer + sizeof(uint8_t), sizeof(int32_t));

    char error_message[ERROR_MESSAGE_SIZE];
    memcpy(error_message, buffer + sizeof(uint8_t) + sizeof(int32_t),
           ERROR_MESSAGE_SIZE);

    if (memcmp(return_code, BOX_SUCCESS, sizeof(uint32_t)) == 0) {
        fprintf(stdout, "OK\n");
    } else {
        fprintf(stdout, "ERROR %s\n", error_message);
    }

    return 0;
}

int list_box_request(char *server_pipe, char *session_pipe_name) {

    // Send the request to the Server

    void *message = calloc(LIST_REQUEST, sizeof(char));
    if (message == NULL) {
        WARN("Unable to alloc memory for request to list boxes.\n");
        return -1;
    }

    memcpy(message, LIST_BOX_R, sizeof(uint8_t));
    message += UINT8_T_SIZE;

    int pipe_n_bytes = strlen(session_pipe_name) > PIPE_NAME_LENGTH
                           ? PIPE_NAME_LENGTH
                           : strlen(session_pipe_name);
    memcpy(message, session_pipe_name, pipe_n_bytes);

    message -= UINT8_T_SIZE;

    if (write(server_pipe, message, LIST_REQUEST) == -1) {
        WARN("Unable to write request message.\n");
        free(message);
        return -1;
    }

    free(message);

    // Read the answer from the Server

    void *buffer = calloc(LIST_RESPONSE, sizeof(char));
    if (buffer == NULL) {
        WARN("Unable to alloc memory to request box action.\n");
        return -1;
    }

    Box *head = NULL;

    int flag = TRUE;
    while (flag) {
        if (read(session_pipe_name, buffer, LIST_RESPONSE) == -1) {
            WARN("Unable to read Box list.\n");
            return -1;
        }

        uint8_t last;
        memcpy(last, buffer, UINT8_T_SIZE);
        buffer += UINT8_T_SIZE;

        if (memcmp(last, BOX_ERROR, UINT8_T_SIZE) == 0) {
            fprintf(stdout, "NO BOXES FOUND\n");
            break;
        } else if (memcmp(last, LAST_BOX, UINT8_T_SIZE) == 0) {
            flag = FALSE;
        }

        char box_name[32];
        memcpy(box_name, buffer, BOX_NAME_LENGTH);
        buffer += BOX_NAME_LENGTH;

        uint64_t box_size;
        memcpy(box_size, buffer, sizeof(uint64_t));
        buffer += sizeof(uint64_t);

        uint64_t n_publishers;
        memcpy(n_publishers, buffer, sizeof(uint64_t));
        buffer += sizeof(uint64_t);

        uint64_t n_subscribers;
        memcpy(n_subscribers, buffer, sizeof(uint64_t));

        if (insertionSort(head, box_name, box_size, n_publishers,
                          n_subscribers) != 0) {
            return -1;
        }
    }

    print_list(head);
    destroy_list(head);

    return 0;
}

int main(int argc, char **argv) {

    if (argc != 5 || argc != 4) {
        WARN("A wrong number of arguments was passed(%d).\n", argc);
        return -1;
    }

    char *server_pipe_name = argv[1];  // Server's Pipe name
    char *session_pipe_name = argv[2]; // Session's Pipe name
    char *request = argv[3];           // Request

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
        return -1;
    }

    switch (*request) {
    case 'create':
        char *box_name = argv[4]; // Box's name

        if (box_request(server_pipe, session_pipe_name, box_name,
                        BOX_CREATION_R) != 0) {
            WARN("Unable to create Box.\n");
            return -1;
        }
        break;
    case 'remove':
        char *box_name = argv[4]; // Box's name

        if (box_request(server_pipe, session_pipe_name, box_name,
                        BOX_REMOVAL_R) != 0) {
            WARN("Unable to remove Box.\n");
            return -1;
        }
        break;
    case 'list':
        if (list_box_request(server_pipe, session_pipe_name) != 0) {
            WARN("Unable to list Boxes.\n");
            return -1;
        }
        break;
    }

    return 0;
}
