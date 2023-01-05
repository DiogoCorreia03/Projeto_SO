/*
*   Servidor
*/

#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {

    if (argc != 3) {
        //erro
    }

    //FIXME?
    if (strcmp(argv[0], "mbroker")) {
        //erro
    }

    //verificar erros na creação
    mkfifo(argv[1], 0666); //server pipe, main pipe onde todos os clients se ligam para comunicar com o server

    (void) argv[2]; //max sessions

    WARN("unimplemented"); // TODO: implement
    return -1;
}
