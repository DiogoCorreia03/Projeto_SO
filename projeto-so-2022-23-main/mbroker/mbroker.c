#include "../fs/operations.h"
#include "../utils/common.h"
#include "logging.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int register_pub(char *buffer, struct Box *head) {
    char *session_pipe_name;
    memcpy(session_pipe_name, buffer, PIPE_NAME_LENGTH);
    buffer += PIPE_NAME_LENGTH;

    char *box_name;
    memcpy(box_name, buffer, BOX_NAME_LENGTH);

    int session_pipe = open(session_pipe_name, O_RDONLY);
    if (session_pipe == -1) {
        WARN("Unable to open Session's Pipe.\n");
        return -1;
    }
    
    Box *box = getBox(head, box_name);
    if (box == NULL) {
        WARN("Box %s doesn't exist.\n", box_name);
        return -1;
    }

    int fhandle = tfs_open(box_name, TFS_O_APPEND);    //Modo de abertura 
    if (fhandle == -1) {
        WARN("Unable to open Box %s.\n", box_name);
        return -1;
    }

    box->file_handle = fhandle;

    return 0;
}

int register_sub(char *buffer, struct Box *head) {
    char *session_pipe_name;
    memcpy(session_pipe_name, buffer, PIPE_NAME_LENGTH);
    buffer += PIPE_NAME_LENGTH;

    char *box_name;
    memcpy(box_name, buffer, BOX_NAME_LENGTH);

    int session_pipe = open(session_pipe_name, O_WRONLY);
    if (session_pipe == -1) {
        WARN("Unable to open Session's Pipe.\n");
        return -1;
    }
    
    Box *box = getBox(head, box_name);
    if (box == NULL) {
        WARN("Box %s doesn't exist.\n", box_name);
        return -1;
    }

    int fhandle = tfs_open(box_name, TFS_O_TRUNC);   //Modo de abertura
    if (fhandle == -1) {
        WARN("Unable to open Box %s.\n", box_name);
        return -1;
    }

    box->file_handle = fhandle;

    return 0;
}

int create_box(char *buffer, struct Box *head) {
    char *session_pipe_name;
    memcpy(session_pipe_name, buffer, PIPE_NAME_LENGTH);
    buffer += PIPE_NAME_LENGTH;

    char *box_name;
    memcpy(box_name, buffer, BOX_NAME_LENGTH);

    int session_pipe = open(session_pipe_name, O_WRONLY);
    if (session_pipe == -1) {
        WARN("Unable to open Session's Pipe.\n");
        return -1;
    }

    int fhandle = tfs_open(box_name, TFS_O_CREAT);
    if (fhandle == -1) {
        WARN("Unable to create Box %s.\n", box_name);
        send_creation_answer(session_pipe, ERROR);
        return -1;
    }

    if (insertBox(head, box_name, fhandle, BOX_NAME_LENGTH) == -1) {
        WARN("Unable to insert Box %s.\n", box_name);
        return -1;
    }

    send_creation_answer
}



void working_thread(char *server_pipe) {

    Box *head = NULL;

    u_int8_t *op_code;
    if (read(server_pipe, op_code, UINT8_T_SIZE) == -1) {
        WARN("Unable to read from Server's Pipe.\n");
        return -1;
    }

    switch (*op_code) {
    case 1:
        char *buffer;
        memset(buffer, 0, REGISTER_LENGTH - UINT8_T_SIZE);

        if (read(server_pipe, buffer, REGISTER_LENGTH - UINT8_T_SIZE) == -1) {
            WARN("Unable to read Session's Pipe.\n");
            return -1;
        }

        if (register_pub(buffer, head) != 0) {
            WARN("Unable to register publisher.\n");
            return -1;
        }
        break;
    
    case 2:
        char *buffer;
        memset(buffer, 0, REGISTER_LENGTH - UINT8_T_SIZE);

        if (read(server_pipe, buffer, REGISTER_LENGTH - UINT8_T_SIZE) == -1) {
            WARN("Unable to read Session's Pipe.\n");
            return -1;
        }

        if (register_sub(buffer, head) != 0) {
            WARN("Unable to register subscriber.\n");
            return -1;
        }
        break;

    case 3:
        char *buffer;
        memset(buffer, 0, REGISTER_LENGTH - UINT8_T_SIZE);

        if (read(server_pipe, buffer, REGISTER_LENGTH - UINT8_T_SIZE) == -1) {
            WARN("Unable to read Session's Pipe.\n");
            return -1;
        }

        create_box(buffer, head);
        break;

    case 5:

        break;

    case 7:

        break;
    }
}

int main(int argc, char **argv) {

    if (argc != 3) {
        WARN("Instead of 3 arguments, %d were passed.\n", argc);
        return -1;
    }

    // veririficar q argv[1] e argv[2] são validos

    // Statrt TFS
    if (tfs_init(NULL) != 0) {
        WARN("Unnable to start TFS.\n");
        return -1;
    }

    // Server's Pipe
    char *server_pipe_name = argv[1];

    char *c = '\0';
    long max_sessions = strtol(argv[2], &c, 10);
    if (errno != 0 || *c != '\0' || max_sessions > INT_MAX ||
        max_sessions < INT_MIN) {
        WARN("Invalid Max Sessions value.\n");
        tfs_destroy(); // FIXME
        return -1;
    }

    if (unlink(server_pipe_name) != 0 && errno != ENOENT) {
        WARN("Unlink(%s) failed: %s\n", server_pipe_name, strerror(errno));
        // FIXME é preciso dar tfs_destroy, close etc?
        return -1;
    }

    if (mkfifo(server_pipe_name, 0777) != 0) {
        WARN("Unnable to create Server's Pipe.\n");
        return -1;
    }

    // Create Working Threads for each Session
    pthread_t sessions_tid[max_sessions];
    for (int i = 0; i < max_sessions; i++) {
        if (pthread_create(&sessions_tid[i], NULL, working_thread, NULL) != 0) {
            WARN("Error creating Thread(%d)\n", i);
            return -1;
        }
    }

    int server_pipe = open(server_pipe_name, O_RDONLY); // FIXME
    if (server_pipe == -1) {
        WARN("Unable to open Server's Pipe.\n");
        return -1;
    }

    if (close(server_pipe) == -1) {
        WARN("Error closing Server's Pipe.\n");
        return -1;
    }

    if (unlink(server_pipe_name) == -1) {
        WARN("Unlink(%s) failed: %s\n", server_pipe_name, strerror(errno));
        return -1;
    }

    if (tfs_destroy() != 0) {
        WARN("Error destroying TFS.\n");
        return -1;
    }

    return 0;
}
