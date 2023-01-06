/*
 *   Servidor
 */

#include "../fs/operations.h"
#include "logging.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {

    if (argc != 3) {
        // erro
    }

    // FIXME?
    if (strcmp(argv[0], "mbroker")) {
        // erro
    }

    // veririficar q argv[1] e argv[2] são validos

    tfs_init(NULL);
    char *server_pipe_name =
        argv[1]; // server pipe, main pipe onde todos os clients
                 // se ligam para comunicar com o server

    int max_sessions = argv[2];

    // FIXME mensagem/tratamento do erro
    if (unlink(server_pipe_name) != 0) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", server_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    // FIXME mensagem/tratamento do erro
    if (mkfifo(server_pipe_name, 0777) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // FIXME mensagem/tratamento do erro, NONBLOCK? como fazer com q continue e
    // não bloqueie no open() ou n é preciso continuar?
    int server_pipe = open(server_pipe_name, O_RDONLY);
    if (server_pipe == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    tfs_destroy();
    return 0;
    // WARN("unimplemented"); // TODO: implement
    // return -1;
}
