/*
 *   Cliente
 */

#include "../fs/operations.h"
#include "logging.h"
#include "string.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {

    if (tfs_init(NULL) != 0) {
        // erro
    }

    if (argc != 4) {
        // erro
    }

    // FIXME
    if (strcmp(argv[0], "pub") != 0) {
        // erro
    }

    char *server_pipe_name = argv[1];  // nome do pipe do servidor
    char *session_pipe_name = argv[2]; // nome do pipe da sessão
    char *box_name = argv[4];          // nome da caixa aonde vai escrever

    if (unlink(server_pipe_name) != 0) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", server_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (unlink(session_pipe_name) != 0) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", session_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Ver permissões
    if (mkfifo(server_pipe_name, 0777) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int server_pipe = open(server_pipe_name, O_WRONLY);
    if (server_pipe == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    size_t size = file_size();

    char *buffer[size];
    memset(buffer, 0, size);

    int c, i = 0;

    while (c = getchar() != EOF) {
        if (i < size) {
            if (c == '/n') {
                c = '\0';
                i = size;
            }
            buffer[i] = c;
            i++;
        }
    }

    // Ver permissões
    if (mkfifo(session_pipe_name, 0777) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int session_pipe = open(session_pipe_name, O_WRONLY);

    if (session_pipe == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (tfs_destroy() != 0) {
        // erro
    }

    return 0;
}
