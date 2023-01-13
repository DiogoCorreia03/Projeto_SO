#include "string.h"
#include "../fs/operations.h"
#include "../utils/common.h"
#include "logging.h"
#include "state.h"
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int box_request(char *server_pipe, char *session_pipe_name, char *box, uint8_t code) {
    void *message = calloc(REGISTER_LENGTH, sizeof(char));

    memcpy(message, code, sizeof(uint8_t));
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

int list_box_request(char *server_pipe, char *session_pipe_name) {
    void *message = calloc(LIST_REQUEST, sizeof(char));

    memcpy(message, LIST_BOX_R, sizeof(uint8_t));
    message += sizeof(uint8_t);

    int pipe_n_bytes = strlen(session_pipe_name) > PIPE_NAME_LENGTH
                           ? PIPE_NAME_LENGTH
                           : strlen(session_pipe_name);
    memcpy(message, session_pipe_name, pipe_n_bytes);

    if (write(server_pipe, message, LIST_REQUEST) == -1) {
        WARN("Unnable to write message.\n");
        free(message);
        return -1;
    }

    free(message);
    return 0;
}


int main(int argc, char **argv) {

    if (argc != 5 || argc != 4) {
        WARN("Instead of 4 or 5 arguments, %d were passed.\n", argc);
        return -1;
    }

    char *server_pipe_name = argv[1];  //nome do pipe do servidor
    char *session_pipe_name = argv[2]; //nome do pipe da sessão

    int server_pipe = open(server_pipe_name, O_WRONLY);
    if (server_pipe == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (unlink(session_pipe_name) != 0 && errno != ENOENT) {
        WARN("Unlink(%s) failed: %s\n", session_pipe_name, strerror(errno));
        return -1;
    }

    if (mkfifo(session_pipe_name, 0777) != 0) {
        WARN("Unnable to create Session's Pipe.\n");
        return -1;
    }

    int session_pipe = open(session_pipe_name, O_RDONLY);
        if (session_pipe == -1) {
            fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

    switch (*argv[3])
    {
    case 'create':
        char *box_name = argv[4]; //nome da box que vai ser criada

        if (box_request(server_pipe, session_pipe_name, box_name, BOX_REMOVAL_R) != 0) {
            WARN("Unable to create Box.\n");
            return -1;
        }

        char *buffer;
        read(session_pipe, buffer, TOTAL_RESPONSE_LENGTH);
        int32_t return_code;
        char error_message;
        memcpy(return_code, buffer + sizeof(uint8_t), sizeof(int32_t));
        memcpy(error_message, buffer + sizeof(uint8_t) + sizeof(int32_t), ERROR_MESSAGE_SIZE);

        if (return_code == 0) {
            fprintf(stdout, "OK\n");
        }
        else {
            fprintf(stdout, "ERROR %s\n", error_message);
        }

        break;
    
    case 'remove':
        char *box_name = argv[4]; //nome da box que vai ser removida

        if (box_request(server_pipe, session_pipe_name, box_name, BOX_REMOVAL_R) != 0) {
            WARN("Unable to remove Box.\n");
            return -1;
        }

        char *buffer;
        read(session_pipe, buffer, TOTAL_RESPONSE_LENGTH);
        int32_t return_code;
        char error_message;
        memcpy(return_code, buffer + sizeof(uint8_t), sizeof(int32_t));
        memcpy(error_message, buffer + sizeof(uint8_t) + sizeof(int32_t), ERROR_MESSAGE_SIZE);

        if (return_code == 0) {
            fprintf(stdout, "OK\n");
        }
        else {
            fprintf(stdout, "ERROR %s\n", error_message);
        }

        break;

    case 'list':
        if (list_box_request(server_pipe, session_pipe_name) != 0) {
            WARN("Unable to list Boxes.\n");
            return -1;
        }

        char *buffer;
        int flag = 0;
        while(flag = 0) {
            read(session_pipe_name, buffer, LIST_RESPONSE);
            buffer += sizeof(uint8_t);

            uint8_t last;
            memcpy(last, buffer, sizeof(u_int8_t));
            if (last == 1)
                flag = 1;
            buffer += sizeof(uint8_t);

            char box_name;
            memcpy(box_name, buffer, BOX_NAME_LENGTH);
            if (strlen(box_name) == 0) {
                fprintf(stdout, "NO BOXES FOUND\n");
                break;
            }
            buffer += BOX_NAME_LENGTH;

            uint64_t box_size;
            memcpy(box_size, buffer, sizeof(uint64_t));
            buffer += sizeof(uint64_t);

            uint64_t n_publishers;
            memcpy(n_publishers, buffer, sizeof(uint64_t));
            buffer += sizeof(uint64_t);

            uint64_t n_subscribers;
            memcpy(n_subscribers, buffer, sizeof(uint64_t));
            
            fprintf(stdout, "%s %zu %zu %zu\n", box_name, box_size, n_publishers, n_subscribers);
        }

        break;
    }

    return 0;
}
