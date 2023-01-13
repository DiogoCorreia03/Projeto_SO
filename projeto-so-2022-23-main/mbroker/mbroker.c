#include "../fs/operations.h"
#include "../producer-consumer/producer-consumer.h"
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

Client_Info *register_client(void *buffer, int session_pipe, Box *head) {

    char box_name[BOX_NAME_LENGTH];
    memset(box_name, 0, BOX_NAME_LENGTH);
    if (read(server_pipe, box_name, BOX_NAME_LENGTH) == -1) {
        WARN("Unable to read request.\n");
        return NULL;
    }

    Client_Info *info = calloc(1, sizeof(Client_Info));
    if (info == NULL) {
        WARN("Unable to alloc memory to create Client.\n");
        return NULL;
    }

    strncpy(info->box_name, box_name, BOX_NAME_LENGTH);
    info->session_pipe = session_pipe;

    return info;
}

int publisher(Client_Info *info, Box *head) {
    void *message = calloc(MESSAGE_SIZE, sizeof(char));
    if (message == NULL) {
        WARN("Unable to alloc memory to read Publisher's message.\n");
        return -1;
    }

    Box *box = getBox(head, info->box_name);
    box->n_publishers++;
    // FIXME box_size?

    int fd = tfs_open(info->box_name, TFS_O_APPEND);
    if (fd == -1) {
        WARN("");
        box->n_publishers--;
        free(message);
        return -1;
    }

    while (TRUE) {
        if (read(info->session_pipe, message, MESSAGE_SIZE) <= 0) {
            WARN("");
            box->n_publishers--;
            free(message);
            return -1;
        }
        message += UINT8_T_SIZE;
        if (tfs_write(fd, message, strlen(message) + 1) == -1) {
            WARN("");
            box->n_publishers--;
            free(message);
            return -1;
        }
    }

    box->n_publishers--;
    free(message);
    return 0;
}

void working_thread(pc_queue_t *queue, Box *head) {
    int run = TRUE;
    while (run) {
        void *buffer = calloc(REQUEST_LENGTH, sizeof(char));
        if (buffer == NULL) {
            WARN("Unable to alloc memory to proccess request.\n");
            return;
        }
        buffer = pcq_dequeue(queue);
        u_int8_t op_code;
        memcpy(op_code, buffer, UINT8_T_SIZE);
        buffer += UINT8_T_SIZE;

        char session_pipe_name[PIPE_NAME_LENGTH];
        memcpy(session_pipe_name, buffer, PIPE_NAME_LENGTH);
        buffer += PIPE_NAME_LENGTH;

        int session_pipe = open(session_pipe_name, O_WRONLY);
        if (session_pipe == -1) {
            WARN("Unable to open Session's Pipe.\n");
            return;
        }

        switch (op_code) {
        case 1:
            Client_Info *info =
                register_client(buffer, session_pipe, head);
            if (info == NULL) {
                WARN("Unable to register publisher.\n");
                return;
            }
            publisher(info, head);
            break;
        case 2:
            if (register_client(buffer, session_pipe, head) == NULL) {
                WARN("Unable to register subscriber.\n");
                return;
            }
            subscriber();
            break;

        case 3:
        case 5:
            if (box_request() != 0) {
                WARN("Unable to process Box request.\n");
                return;
            }
            break;

        case 7:
            char *message;
            memset(buffer, 0, PIPE_NAME_LENGTH);

            if (read(server_pipe, message, PIPE_NAME_LENGTH) == -1) {
                WARN("Unable to read Session's Pipe.\n");
                return;
            }

            break;
        }
    }
}

int main(int argc, char **argv) {

    if (argc != 3) {
        WARN("Instead of 3 arguments, %d were passed.\n", argc);
        return -1;
    }

    // Start TFS
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
        tfs_destroy();
        return -1;
    }

    if (unlink(server_pipe_name) != 0 && errno != ENOENT) {
        WARN("Unlink(%s) failed: %s\n", server_pipe_name, strerror(errno));
        tfs_destroy();
        return -1;
    }

    if (mkfifo(server_pipe_name, 0777) != 0) {
        WARN("Unable to create Server's Pipe.\n");
        tfs_destroy();
        return -1;
    }

    Box *head = NULL; // Linked list to store all the Boxes that are created

    pc_queue_t *queue = malloc(sizeof(pc_queue_t));
    pcq_create(queue, QUEUE_CAPACITY);

    // Create Worker Threads for each Session
    pthread_t sessions_tid[max_sessions];
    for (int i = 0; i < max_sessions; i++) {
        if (pthread_create(&sessions_tid[i], NULL, working_thread,
                           NULL /*FIXME add variables*/) != 0) {
            WARN("Error creating Thread(%d)\n", i);
            tfs_destroy();
            unlink(server_pipe_name);
            return -1;
        }
    }

    int server_pipe = open(server_pipe_name, O_RDONLY);
    if (server_pipe == -1) {
        WARN("Unable to open Server's Pipe.\n");
        tfs_destroy();
        unlink(server_pipe_name);
        return -1;
    }

    int run = TRUE;
    void *message = calloc(REQUEST_LENGTH, sizeof(char));
    while (run) {
        if (read(server_pipe, message, REQUEST_LENGTH) == -1) {
            WARN("Unable to read message to Server Pipe.\n");
            free(message);
            tfs_destroy();
            close(server_pipe);
            unlink(server_pipe_name);
            return -1;
        }

        if (pcq_enqueue(queue, message) == -1) {
            WARN("Unable to queue request.\n");
            free(message);
            tfs_destroy();
            close(server_pipe);
            unlink(server_pipe_name);
            return -1;
        }

        memset(message, 0, REQUEST_LENGTH);
    }

    free(message);

    for (int i = 0; i < max_sessions; i++) {
        if (pthread_join(&sessions_tid[i], NULL) != 0) { // FIXME
            WARN("Error creating Thread(%d)\n", i);
            tfs_destroy();
            close(server_pipe);
            unlink(server_pipe_name);
            return -1;
        }
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

/*char buffer[BOX_NAME_LENGTH];
        memset(buffer, 0, BOX_NAME_LENGTH);

        if (read(server_pipe, buffer, BOX_NAME_LENGTH) == -1) {
            WARN("Unable to read request.\n");
            return -1;
        }

        if (register_pub(buffer, head) != 0) {
            WARN("Unable to register publisher.\n");
            return -1;
        }
        break;

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
        break;char *buffer;
        memset(buffer, 0, REQUEST_LENGTH - UINT8_T_SIZE);

        if (read(server_pipe, buffer, REQUEST_LENGTH - UINT8_T_SIZE) == -1) {
            WARN("Unable to read Session's Pipe.\n");
            return -1;
        }

        if (remove_box(buffer, head, op_code) != 0) {
            WARN("Unable to remove Box.\n");
            return -1;
        }


        */

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

    if (deleteBox(head, box_name) == -1) {
        WARN("Unable to delete Box %s.\n", box_name);
        return -1;
    }

    if (box_answer(session_pipe, BOX_SUCCESS, op_code) == -1) {
        WARN("Unable to send answer to Session's Pipe.\n");
        return -1;
    }

    return 0;
}

int box_answer(int session_pipe, int32_t return_code, uint8_t op_code) {
    void *message = calloc(TOTAL_RESPONSE_LENGTH, sizeof(char));
    if (message == NULL) {
        WARN("Unable to alloc memory to send Message.\n");
        return -1;
    }

    switch (op_code) {
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
        memcpy(message, "ERROR: Unable to process request.\n",
               ERROR_MESSAGE_RESPONSE_SIZE);
    }

    if (write(session_pipe, message, TOTAL_RESPONSE_LENGTH) == -1) {
        WARN("Unable to write in Session's Pipe.\n");
        return -1;
    }

    return 0;
}