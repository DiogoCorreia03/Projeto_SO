/*
*   Cliente
*/

#include "string.h"
#include "../fs/operations.h"
#include "logging.h"
#include "state.h"
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
#define TOTAL_RESPONSE_LENGTH (1029)
#define RESPONSE_INDEX (2)
#define ERROR_MESSAGE_SIZE (1024)

void register_box(char *server_pipe, char *session_pipe_name, char *box_name) {
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

    char request[TOTAL_REGISTER_LENGTH];
    u_int8_t code = 3;
    strcpy(request, (void*) code);
    strcat(request, client_named_pipe_path);
    strcat(request, box_name_copy);

    write(server_pipe, request, strlen(request));
}


int main(int argc, char **argv) {

    if (tfs_init(NULL) != 0) {
        //erro
    }

    if (argc != 5 || argc != 4) {
        //erro
    }

    if (strcmp(argv[0], "manager") != 0) {
        //erro
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

        register_box(server_pipe, session_pipe_name, box_name);

        char *buffer;
        int session_pipe = open(session_pipe_name, O_RDONLY);
        if (session_pipe == -1) {
            fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        read(session_pipe, buffer, TOTAL_RESPONSE_LENGTH);
        int32_t return_code;
        char error_message;
        memcpy(return_code, buffer + sizeof(u_int8_t), sizeof(u_int32_t));
        memcpy(error_message, buffer + sizeof(u_int8_t) + sizeof(u_int32_t), ERROR_MESSAGE_SIZE);

        if (return_code == 0) {
            if (tfs_open(box_name, O_CREAT) == -1) {   //cria uma caixa
            //erro
            }

            fprintf(stdout, "OK\n");
        }
        else {
            fprintf(stdout, "ERROR %s\n", error_message);
        }
        
        break;
    
    case 'remove':
        char *box_name = argv[4]; //nome da box que vai ser removida

        if (unlink(box_name) != 0) {
            fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", box_name,
                strerror(errno));
        exit(EXIT_FAILURE);
        }
        //Já vejo
        break;

    case 'list':

        break;
    }

    if (tfs_destroy() != 0) {
        //erro
    }
    
    return 0;
}
