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

uint8_t BOX_CREATION_R = 3;
uint8_t BOX_CREATION_A = 4;
uint8_t BOX_REMOVAL_R = 5;
uint8_t BOX_REMOVAL_A = 6;
uint8_t LIST_BOX_R = 7;
uint8_t LIST_BOX_A = 8;
uint8_t SERVER_2_SUB = 10;
int32_t BOX_SUCCESS = 0;
int32_t BOX_ERROR = -1;
uint8_t LAST_BOX = 1;

Client_Info *register_client(void *buffer, int session_pipe) {

    char box_name[BOX_NAME_LENGTH];
    memset(box_name, 0, BOX_NAME_LENGTH);
    memcpy(box_name, buffer, BOX_NAME_LENGTH);

    Client_Info *info = calloc(1, sizeof(Client_Info));
    if (info == NULL) {
        WARN("Unable to alloc memory to create Client.\n");
        return NULL;
    }

    strncpy(info->box_name, box_name, BOX_NAME_LENGTH);
    info->session_pipe = session_pipe;

    return info;
}

int publisher(Client_Info *info, struct Box *head) {
    void *message = calloc(MESSAGE_SIZE, sizeof(char));
    if (message == NULL) {
        WARN("Unable to alloc memory to read Publisher's message.\n");
        return -1;
    }

    struct Box *box = getBox(head, info->box_name);
    if (box == NULL) {
        WARN("Box not found.\n");
        return -1;
    }

    if (box->n_publishers != 0) {
        return -1;
    }

    box->n_publishers++;

    int fd = tfs_open(info->box_name, TFS_O_APPEND);
    if (fd == -1) {
        WARN("Unable to open TFS file.\n");
        box->n_publishers--;
        free(message);
        return -1;
    }

    uint64_t bytes_written;

    while (TRUE) {
        if (read(info->session_pipe, message, MESSAGE_SIZE) <= 0) {
            WARN("Error reading message from Publisher's Pipe.\n");
            box->n_publishers--;
            free(message);
            return -1;
        }
        message += UINT8_T_SIZE;
        if ((bytes_written =
                 (uint64_t)tfs_write(fd, message, strlen(message) + 1)) == -1) {
            WARN("Error writing message into Box.\n");
            box->n_publishers--;
            free(message);
            return -1;
        }
        box->box_size += bytes_written;
        if (bytes_written != strlen(message) + 1) {
            WARN("Unable to write whole message, Box full.\n");
        }
    }

    box->n_publishers--;
    free(message);
    return 0;
}

int subscriber(Client_Info *info, struct Box *head) {
    void *message = calloc(MESSAGE_SIZE, sizeof(char));
    if (message == NULL) {
        WARN("Unable to alloc memory to read from Box.\n");
        return -1;
    }

    struct Box *box = getBox(head, info->box_name);
    if (box == NULL) {
        WARN("Box not found.\n");
        return -1;
    }

    box->n_subscribers++;

    int fd = tfs_open(info->box_name, TFS_O_TRUNC);
    if (fd == -1) {
        WARN("Unable to open TFS file.\n");
        box->n_subscribers--;
        free(message);
        return -1;
    }

    char *buffer = calloc(BLOCK_SIZE, sizeof(char));
    if (buffer == NULL) {
        WARN("Unable to alloc memory to create buffer.\n");
        box->n_subscribers--;
        free(message);
        return -1;
    }

    memcpy(message, &SERVER_2_SUB, UINT8_T_SIZE);
    message += UINT8_T_SIZE;

    while (TRUE) {

        if (tfs_read(fd, buffer, BLOCK_SIZE) == -1) {
            WARN("Unable to read message from Box.\n");
            box->n_subscribers--;
            free(message);
            free(buffer);
            return -1;
        }

        size_t i;
        for (i = 0; i < BLOCK_SIZE; i++) {
            if (buffer[i] == '\0') {
                memcpy(message, buffer, i);
                break;
            }
        }

        if (write(info->session_pipe, message - UINT8_T_SIZE,
                  strlen(message) + 1)) {
            WARN("Unable to write in Session's Pipe.\n");
            box->n_subscribers--;
            free(message);
            free(buffer);
            return -1;
        }

        memset(message, 0, MESSAGE_SIZE);
        memcpy(message, &SERVER_2_SUB, UINT8_T_SIZE);
        message += UINT8_T_SIZE;
        memcpy(message, buffer + i, BLOCK_SIZE - i);
        message += BLOCK_SIZE - i;
        memset(buffer, 0, BLOCK_SIZE);
    }

    box->n_subscribers--;
    free(message);
    free(buffer);
    return 0;
}

int box_answer(int session_pipe, int32_t return_code, uint8_t op_code) {
    void *message = calloc(TOTAL_RESPONSE_LENGTH, sizeof(char));
    if (message == NULL) {
        WARN("Unable to alloc memory to send Message.\n");
        free(message);
        return -1;
    }

    switch (op_code) {
    case 3:
        memcpy(message, &BOX_CREATION_A, UINT8_T_SIZE);
        break;

    case 5:
        memcpy(message, &BOX_REMOVAL_A, UINT8_T_SIZE);
        break;

    default:
        WARN("Unknown OP_CODE given.\n");
    }
    message += UINT8_T_SIZE;

    memcpy(message, &return_code, sizeof(int32_t));
    message += sizeof(int32_t);

    char error_message[] = "ERROR: Unable to process request.\n";
    if (return_code == BOX_ERROR) {
        memcpy(message, error_message, strlen(error_message));
    }

    if (write(session_pipe, message, strlen(error_message)) == -1) {
        WARN("Unable to write in Session's Pipe.\n");
        message -= (UINT8_T_SIZE + sizeof(int32_t));
        free(message);
        return -1;
    }

    message -= (UINT8_T_SIZE + sizeof(int32_t));
    free(message);
    return 0;
}

int create_box(int session_pipe, void *buffer, struct Box *head,
               uint8_t op_code) {

    char box_name[BOX_NAME_LENGTH];
    memset(box_name, 0, BOX_NAME_LENGTH);
    memcpy(box_name, buffer, BOX_NAME_LENGTH);

    int fhandle = tfs_open(box_name, TFS_O_CREAT);
    if (fhandle == -1) {
        WARN("Unable to create Box %s.\n", box_name);
        box_answer(session_pipe, BOX_ERROR, op_code);
        return -1;
    }

    if (insertBox(head, box_name, BOX_NAME_LENGTH) == -1) {
        WARN("Unable to insert Box %s.\n", box_name);
        return -1;
    }

    if (box_answer(session_pipe, BOX_SUCCESS, op_code) == -1) {
        WARN("Unable to send answer to Session's Pipe.\n");
        return -1;
    }

    return 0;
}

int remove_box(int session_pipe, void *buffer, struct Box *head,
               uint8_t op_code) {

    char box_name[BOX_NAME_LENGTH];
    memset(box_name, 0, BOX_NAME_LENGTH);
    memcpy(box_name, buffer, BOX_NAME_LENGTH);

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

int list_box(int session_pipe, struct Box *head) {
    char *buffer;
    memset(buffer, 0, LIST_RESPONSE);

    memcpy(buffer, LIST_BOX_A, UINT8_T_SIZE);
    buffer += UINT8_T_SIZE;

    uint8_t last;
    struct Box *prev = NULL;
    struct Box *current = head;
    while(current != NULL) {
        last = 0;
        box_to_string(prev, buffer, last);
        if (write(session_pipe, buffer, LIST_RESPONSE) == -1) {
            WARN("Unable to write in Session's Pipe.\n");
            return -1;
        }

        current = current->next;
        prev = current;

        memset(buffer, 0, LIST_RESPONSE);
        memcpy(buffer, LIST_BOX_A, UINT8_T_SIZE);
        buffer += UINT8_T_SIZE;
    }

    last = 1;
    box_to_string(current, buffer, last);
    if (write(session_pipe, buffer, LIST_RESPONSE) == -1) {
        WARN("Unable to write in Session's Pipe.\n");
        return -1;
    }

    return 0;
}

void *working_thread(void *_args) {
    thread_args *args = (thread_args *)_args;
    int run = TRUE;
    while (run) {
        void *buffer = calloc(REQUEST_LENGTH, sizeof(char));
        if (buffer == NULL) {
            WARN("Unable to alloc memory to proccess request.\n");
            return 0;
        }
        buffer = pcq_dequeue(args->queue);
        u_int8_t op_code;
        memcpy(&op_code, buffer, UINT8_T_SIZE);
        buffer += UINT8_T_SIZE;

        char session_pipe_name[PIPE_NAME_LENGTH];
        memcpy(session_pipe_name, buffer, PIPE_NAME_LENGTH);
        buffer += PIPE_NAME_LENGTH;

        int session_pipe = open(session_pipe_name, O_WRONLY);
        if (session_pipe == -1) {
            WARN("Unable to open Session's Pipe.\n");
            return 0;
        }

        Client_Info *info;

        switch (op_code) {
        case 1:
            info = register_client(buffer, session_pipe);
            if (info == NULL) {
                WARN("Unable to register publisher.\n");
            }
            if (publisher(info, args->head) == -1) {
                WARN("Publisher unable to write.\n")
            }
            break;

        case 2:
            info = register_client(buffer, session_pipe);
            if (info == NULL) {
                WARN("Unable to register publisher.\n");
            }
            if (subscriber(info, args->head) == -1) {
                WARN("Subscriber unable to read.\n");
            }
            break;

        case 3:
            if (create_box(session_pipe, buffer, args->head, op_code) == -1) {
                WARN("Unable to create Box-\n");
            }
            break;

        case 5:
            if (remove_box(session_pipe, buffer, args->head, op_code) == -1) {
                WARN("Unable to remove Box.\n");
            }
            break;

        case 7:
            if (list_box(session_pipe, args->head) == -1) {
                WARN("Unable to list boxes.\n");
            }
            break;

        default:
            WARN("Unknown OP_CODE given.\n");
        }

        buffer -= (UINT8_T_SIZE + PIPE_NAME_LENGTH);
        free(buffer);
    }
    return 0;
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

    // Linked list to store all the Boxes that are created
    struct Box *head = NULL;

    pc_queue_t *queue = malloc(sizeof(pc_queue_t));
    pcq_create(queue, QUEUE_CAPACITY);

    // Create Worker Threads for each Session
    pthread_t sessions_tid[max_sessions];
    thread_args *args = malloc(sizeof(thread_args));
    args->queue = queue;
    args->head = head;
    for (int i = 0; i < max_sessions; i++) {
        if (pthread_create(&sessions_tid[i], NULL, working_thread, args) != 0) {
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
        if (pthread_join(sessions_tid[i], NULL) != 0) {
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
/*

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