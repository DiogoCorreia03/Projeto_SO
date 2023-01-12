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

void box_request(char *server_pipe, char *session_pipe_name, char *box, uint8_t code) {
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

    switch (*argv[3])
    {
    case 'create':
        char *box_name = argv[4]; //nome da box que vai ser criada

        box_request(server_pipe, session_pipe_name, box_name, BOX_CREATION_R);

        char *buffer;
        int session_pipe = open(session_pipe_name, O_RDONLY);
        if (session_pipe == -1) {
            fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

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

        box_request(server_pipe, session_pipe_name, box_name, BOX_REMOVAL_R);

        char *buffer;
        int session_pipe = open(session_pipe_name, O_RDONLY);
        if (session_pipe == -1) {
            fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

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

        break;
    }

    if (tfs_destroy() != 0) {
        //erro
    }
    
    return 0;
}
