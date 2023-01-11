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

#define MAX_CLIENT_NAME_PIPE_PATH (256)
#define MAX_BOX_NAME (32)
#define TOTAL_REGISTER_LENGTH (289)
#define TOTAL_MESSAGE_LENGTH (1025)

void register_pub(char *server_pipe, char *session_pipe_name, char *box_name) {
    char client_named_pipe_path[MAX_CLIENT_NAME_PIPE_PATH], box_name_copy[MAX_BOX_NAME];

    memset(client_named_pipe_path, 0, MAX_CLIENT_NAME_PIPE_PATH);
    memset(box_name_copy, 0, MAX_BOX_NAME);

    int n_pipe_name_size = strlen(session_pipe_name);
    int n_box_name_size = strlen(box_name);

    if (n_pipe_name_size > MAX_CLIENT_NAME_PIPE_PATH)
        n_pipe_name_size = MAX_CLIENT_NAME_PIPE_PATH;

    if (n_box_name_size > MAX_BOX_NAME)
        n_box_name_size = MAX_BOX_NAME;

    memcpy(client_named_pipe_path, session_pipe_name, strlen(session_pipe_name));
    memcpy(box_name_copy, box_name, strlen(box_name));

    char request[289];
    u_int8_t code = 1;
    strcpy(request, (void*) code);
    strcat(request, client_named_pipe_path);
    strcat(request, box_name_copy);

    write(server_pipe, request, strlen(request));
}

void send_message(char *server_pipe, char *buffer) {
    char *message[TOTAL_MESSAGE_LENGTH];
    u_int8_t code = 9;
    strcpy(message, (void*) code);
    strcat(message, buffer);

    write(server_pipe, message, strlen(message));
}

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

    char *server_pipe_name = argv[2]; 
    char *session_pipe_name = argv[3]; // nome do pipe da sessão
    char *box_name = argv[4];          // nome da caixa aonde vai escrever

    int server_pipe = open(server_pipe_name, O_WRONLY);
    if (server_pipe == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    register_pub(server_pipe, session_pipe_name, box_name);     //Request enviado ao servidor


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

    send_message(server_pipe, buffer);

    if (unlink(session_pipe_name) != 0) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", session_pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);
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
        //erro
    }

    return 0;
}
