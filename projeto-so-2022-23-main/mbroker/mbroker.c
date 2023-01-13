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

int box_answer(int session_pipe, int32_t return_code, uint8_t op_code) {
    void *message = calloc(TOTAL_RESPONSE_LENGTH, sizeof(char));
    if (message == NULL) {
        WARN("Unable to alloc memory to send Message.\n");
        return -1;
    }
    
    switch (op_code)
    {
    case BOX_CREATION_R:
        memcpy(message, BOX_CREATION_A, UINT8_T_SIZE);
        break;

    case BOX_REMOVAL_R:
        memcpy(message, BOX_REMOVAL_A, UINT8_T_SIZE);

        break;
    }
    message += UINT8_T_SIZE;

    memcpy(message, return_code, sizeof(int32_t));
    message += sizeof(int32_t);

    if (return_code == BOX_ERROR) {
        char error_message[ERROR_MESSAGE_RESPONSE_SIZE];
        memcpy(message, "ERROR: Unable to process request.\n", ERROR_MESSAGE_RESPONSE_SIZE);
    }

    if (write(session_pipe, message, TOTAL_RESPONSE_LENGTH) == -1) {
        WARN("Unable to write in Session's Pipe.\n");
        return -1;
    }
    
    return 0;
}



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

int create_box(char *buffer, struct Box *head, uint8_t op_code) {
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
        box_answer(session_pipe, BOX_ERROR, op_code);
        return -1;
    }

    if (insertBox(head, box_name, fhandle, BOX_NAME_LENGTH) == -1) {
        WARN("Unable to insert Box %s.\n", box_name);
        return -1;
    }

    if (box_answer(session_pipe, BOX_SUCCESS, op_code) == -1) {
        WARN("Unable to send answer to Session's Pipe.\n");
        return -1;
    }

    return 0;
}

int remove_box(char *buffer, struct Box *head, uint8_t op_code) {
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

    if (unlink(box_name) == -1) {
        WARN("Unable to unlink Box %s.\n", box_name);
        box_answer(session_pipe, BOX_ERROR, op_code);
        return -1;
    }

    if(deleteBox(head, box_name) == -1) {
        WARN("Unable to delete Box %s.\n", box_name);
        return -1;
    }

    if (box_answer(session_pipe, BOX_SUCCESS, op_code) == -1) {
        WARN("Unable to send answer to Session's Pipe.\n");
        return -1;
    }

    return 0;
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
        memset(buffer, 0, REQUEST_LENGTH - UINT8_T_SIZE);

        if (read(server_pipe, buffer, REQUEST_LENGTH - UINT8_T_SIZE) == -1) {
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
        memset(buffer, 0, REQUEST_LENGTH - UINT8_T_SIZE);

        if (read(server_pipe, buffer, REQUEST_LENGTH - UINT8_T_SIZE) == -1) {
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
        memset(buffer, 0, REQUEST_LENGTH - UINT8_T_SIZE);

        if (read(server_pipe, buffer, REQUEST_LENGTH - UINT8_T_SIZE) == -1) {
            WARN("Unable to read Session's Pipe.\n");
            return -1;
        }

        if (create_box(buffer, head, op_code) != 0) {
            WARN("Unable to create box.\n");
            return -1;
        }
        break;

    case 5:
        char *buffer;
        memset(buffer, 0, REQUEST_LENGTH - UINT8_T_SIZE);

        if (read(server_pipe, buffer, REQUEST_LENGTH - UINT8_T_SIZE) == -1) {
            WARN("Unable to read Session's Pipe.\n");
            return -1;
        }

        if (remove_box(buffer, head, op_code) != 0) {
            WARN("Unable to remove Box.\n");
            return -1;
        }
        break;

    case 7:
        char *buffer;
        memset(buffer, 0, PIPE_NAME_LENGTH);

        if (read(server_pipe, buffer, PIPE_NAME_LENGTH) == -1) {
            WARN("Unable to read Session's Pipe.\n");
            return -1;
        }

        

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
        WARN("Unable to start TFS.\n");
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
        WARN("Unable to create Server's Pipe.\n");
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
