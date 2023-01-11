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


/*static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> list\n");
}*/

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

    switch (*argv[3])
    {
    case 'create':
        char *box_name = argv[4]; //nome da box que vai ser criada

        if (tfs_open(box_name, O_CREAT) == -1) {   //cria uma caixa
            //erro
        }
        //Já vejo
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
        for (int i = 0; i < _open_file_entry_size(); i++) {
            printf("%s\n", get_open_file_entry(i));            //super errado e incompleto mas é só para ter uma ideia
        }

        break;
    }

    if (tfs_destroy() != 0) {
        //erro
    }
    
    return 0;
}
