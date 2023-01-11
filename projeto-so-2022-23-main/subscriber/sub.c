/*
 *   Cliente
 */

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

void register_sub(int server_pipe, char *session_pipe_name, char *box) {
    void *message = calloc(REGISTER_LENGTH, sizeof(char));

    memcpy(message, SUB_REGISTER, sizeof(uint8_t));
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
        // erro
    }
}

int main(int argc, char **argv) {

    if (tfs_init(NULL) != 0) {
        // erro
    }

    if (argc != 4) {
        // erro
    }

    // FIXME
    if (strcmp(argv[0], "sub") != 0) {
        // erro
    }

    char *server_pipe_name = argv[1];  // nome do pipe do servidor
    char *session_pipe_name = argv[2]; // nome do pipe da sessão
    char *box_name = argv[4];          // nome da caixa aonde vai ler

    int server_pipe = open(server_pipe_name, O_WRONLY);
    // FIXME erro
    if (server_pipe == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // FIXME erro
    if (unlink(session_pipe_name) != 0) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", server_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);
    }
    // Ver permissões  FIXME erro
    if (mkfifo(session_pipe_name, 0777) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    register_sub(server_pipe, session_pipe_name, box_name);

    int session_pipe = open(session_pipe_name, O_RDONLY);
    // FIXME erro
    if (session_pipe == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (tfs_destroy() != 0) {
        // erro
    }

    return 0;
}
