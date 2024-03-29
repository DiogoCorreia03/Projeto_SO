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

Client_Info *register_client(void *buffer, int session_pipe) {

    char box_name[BOX_NAME_LENGTH];
    memset(box_name, 0, BOX_NAME_LENGTH);
    memcpy(box_name, buffer, BOX_NAME_LENGTH);

    Client_Info *info = calloc(1, sizeof(Client_Info));
    if (info == NULL) {
        fprintf(stderr,"Unable to alloc memory to create Client.\n");
        return NULL;
    }

    strncpy(info->box_name, box_name, BOX_NAME_LENGTH);
    info->session_pipe = session_pipe;

    return info;
}

int publisher(Client_Info *info, struct Box *head) {
    void *message = calloc(MESSAGE_SIZE + UINT8_T_SIZE, sizeof(char));
    if (message == NULL) {
        fprintf(stderr,"Unable to alloc memory to read Publisher's message.\n");
        return -1;
    }

    struct Box *box = getBox(head, info->box_name);
    if (box == NULL) {
        fprintf(stderr,"Box not found.\n");
        return -1;
    }

    if (box->n_publishers != 0) {
        return -1;
    }

    box->n_publishers++;

    int fd = tfs_open(info->box_name, TFS_O_APPEND);
    if (fd == -1) {
        fprintf(stderr,"Unable to open TFS file.\n");
        box->n_publishers--;
        free(message);
        return -1;
    }

    uint64_t bytes_written;

    while (TRUE) {
        if (read(info->session_pipe, message, MESSAGE_SIZE + UINT8_T_SIZE) <=
            0) {
            fprintf(stderr,"Error reading message from Publisher's Pipe.\n");
            box->n_publishers--;
            free(message);
            tfs_close(fd);
            return -1;
        }
        message += UINT8_T_SIZE;
        if ((bytes_written =
                 (uint64_t)tfs_write(fd, message, strlen(message) + 1)) == -1) {
            fprintf(stderr,"Error writing message into Box.\n");
            box->n_publishers--;
            free(message);
            tfs_close(fd);
            return -1;
        }
        box->box_size += bytes_written;
        if (bytes_written != strlen(message) + 1) {
            fprintf(stderr,"Unable to write whole message, Box full.\n");
        }
    }

    box->n_publishers--;
    free(message);
    tfs_close(fd);
    return 0;
}

int subscriber(Client_Info *info, struct Box *head) {
    void *message = calloc(MESSAGE_SIZE + UINT8_T_SIZE, sizeof(char));
    if (message == NULL) {
        fprintf(stderr,"Unable to alloc memory to read from Box.\n");
        return -1;
    }

    struct Box *box = getBox(head, info->box_name);
    if (box == NULL) {
        fprintf(stderr,"Box not found.\n");
        return -1;
    }

    box->n_subscribers++;

    int fd = tfs_open(info->box_name, TFS_O_TRUNC);
    if (fd == -1) {
        fprintf(stderr,"Unable to open TFS file.\n");
        box->n_subscribers--;
        free(message);
        return -1;
    }

    char *buffer = calloc(MESSAGE_SIZE, sizeof(char));
    if (buffer == NULL) {
        fprintf(stderr,"Unable to alloc memory to create buffer.\n");
        box->n_subscribers--;
        free(message);
        tfs_close(fd);
        return -1;
    }

    memcpy(message, &SERVER_2_SUB, UINT8_T_SIZE);
    message += UINT8_T_SIZE;

    while (TRUE) {

        if (tfs_read(fd, buffer, MESSAGE_SIZE) == -1) {
            fprintf(stderr,"Unable to read message from Box.\n");
            box->n_subscribers--;
            free(message);
            free(buffer);
            tfs_close(fd);
            return -1;
        }

        size_t i;
        for (i = 0; i < MESSAGE_SIZE; i++) {
            if (buffer[i] == '\0') {
                memcpy(message, buffer, i);
                break;
            }
        }

        if (write(info->session_pipe, message - UINT8_T_SIZE,
                  strlen(message) + 1)) {
            fprintf(stderr,"Unable to write in Session's Pipe.\n");
            box->n_subscribers--;
            free(message);
            free(buffer);
            tfs_close(fd);
            return -1;
        }

        memset(message, 0, MESSAGE_SIZE + UINT8_T_SIZE);
        memcpy(message, &SERVER_2_SUB, UINT8_T_SIZE);
        message += UINT8_T_SIZE;
        memcpy(message, buffer + i, MESSAGE_SIZE - i);
        message += MESSAGE_SIZE - i;
        memset(buffer, 0, MESSAGE_SIZE);
    }

    box->n_subscribers--;
    free(message);
    free(buffer);
    tfs_close(fd);
    return 0;
}

int box_answer(int session_pipe, int32_t return_code, uint8_t op_code) {
    void *message = calloc(TOTAL_RESPONSE_LENGTH, sizeof(char));
    if (message == NULL) {
        fprintf(stderr,"Unable to alloc memory to send Message.\n");
        return -1;
    }

    switch (op_code) {
    case BOX_CREATION_R:
        memcpy(message, &BOX_CREATION_A, UINT8_T_SIZE);
        break;

    case BOX_REMOVAL_R:
        memcpy(message, &BOX_REMOVAL_A, UINT8_T_SIZE);
        break;

    default:
        fprintf(stderr,"Unknown OP_CODE given.\n");
    }
    message += UINT8_T_SIZE;

    memcpy(message, &return_code, sizeof(int32_t));
    message += sizeof(int32_t);

    char error_message[] = "ERROR: Unable to process request.";
    if (return_code == BOX_ERROR) {
        memcpy(message, error_message, strlen(error_message));
    }

    if (write(session_pipe, message, strlen(error_message)) == -1) {
        fprintf(stderr,"Unable to write in Session's Pipe.\n");
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
        fprintf(stderr,"Unable to create Box %s.\n", box_name);
        box_answer(session_pipe, BOX_ERROR, op_code);
        return -1;
    }

    if (tfs_close(fhandle) == -1) {
        box_answer(session_pipe, BOX_ERROR, op_code);
        return -1;
    }

    if (insertBox(head, box_name, BOX_NAME_LENGTH) == -1) {
        box_answer(session_pipe, BOX_ERROR, op_code);
        fprintf(stderr,"Unable to insert Box %s.\n", box_name);
        return -1;
    }

    if (box_answer(session_pipe, BOX_SUCCESS, op_code) == -1) {

        fprintf(stderr,"Unable to send answer to Session's Pipe.\n");
        return -1;
    }

    return 0;
}

int remove_box(int session_pipe, void *buffer, struct Box *head,
               uint8_t op_code) {

    char box_name[BOX_NAME_LENGTH];
    memset(box_name, 0, BOX_NAME_LENGTH);
    memcpy(box_name, buffer, BOX_NAME_LENGTH);

    if (tfs_unlink(box_name) == -1) {
        fprintf(stderr,"Unable to unlink Box %s.\n", box_name);
        box_answer(session_pipe, BOX_ERROR, op_code);
        return -1;
    }

    if (deleteBox(head, box_name) == -1) {
        fprintf(stderr,"Unable to delete Box %s.\n", box_name);
        box_answer(session_pipe, BOX_ERROR, op_code);
        return -1;
    }

    if (box_answer(session_pipe, BOX_SUCCESS, op_code) == -1) {
        fprintf(stderr,"Unable to send answer to Session's Pipe.\n");
        box_answer(session_pipe, BOX_ERROR, op_code);
        return -1;
    }

    return 0;
}

int list_box(int session_pipe, struct Box *head) {
    void *buffer = calloc(LIST_RESPONSE, sizeof(char));
    if (buffer == NULL) {
        fprintf(stderr,"Unable to alloc memory to list Boxes.\n");
        return -1;
    }

    memcpy(buffer, &LIST_BOX_A, UINT8_T_SIZE);

    struct Box *current = head;
    if (head == NULL) {
        struct Box *no_box = malloc(sizeof(struct Box));
        if (no_box == NULL) {
            fprintf(stderr,"Unable to alloc memory to list Boxes.\n");
            free(buffer);
            return -1;
        }
        memset(no_box->box_name, 0, BOX_NAME_LENGTH);
        no_box->box_size = 0;
        no_box->n_publishers = 0;
        no_box->n_subscribers = 0;
        no_box->last = 1;
        no_box->next = NULL;
        box_to_string(no_box, buffer + UINT8_T_SIZE);
        if (write(session_pipe, buffer, LIST_RESPONSE) == -1) {
            fprintf(stderr,"Unable to write in Manager's Pipe.\n");
            free(no_box);
            free(buffer);
            return -1;
        }
        free(no_box);
    }

    while (current != NULL) {
        box_to_string(current, buffer + UINT8_T_SIZE);
        if (write(session_pipe, buffer, LIST_RESPONSE) == -1) {
            fprintf(stderr,"Unable to write in Manager's Pipe.\n");
            buffer -= UINT8_T_SIZE;
            free(buffer);
            return -1;
        }

        current = current->next;

        memset(buffer, 0, LIST_RESPONSE);
        memcpy(buffer, &LIST_BOX_A, UINT8_T_SIZE);
    }

    free(buffer);

    return 0;
}

void *working_thread(void *_args) {
    thread_args *args = (thread_args *)_args;
    int run = TRUE;
    while (run) {
        void *buffer = calloc(REQUEST_LENGTH, sizeof(char));
        if (buffer == NULL) {
            fprintf(stderr,"Unable to alloc memory to proccess request.\n");
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
            fprintf(stderr,"Unable to open Session's Pipe.\n");
            return 0;
        }

        Client_Info *info;

        switch (op_code) {
        case 1:
            info = register_client(buffer, session_pipe);
            if (info == NULL) {
                fprintf(stderr,"Unable to register publisher.\n");
                close(session_pipe);
            }
            if (publisher(info, args->head) == -1) {
                fprintf(stderr,"Publisher unable to write.\n");
                close(session_pipe);
            }
            break;

        case 2:
            info = register_client(buffer, session_pipe);
            if (info == NULL) {
                fprintf(stderr,"Unable to register publisher.\n");
                close(session_pipe);
            }
            if (subscriber(info, args->head) == -1) {
                fprintf(stderr,"Subscriber unable to read.\n");
                close(session_pipe);
            }
            break;

        case 3:
            if (create_box(session_pipe, buffer, args->head, op_code) == -1) {
                fprintf(stderr,"Unable to create Box-\n");
                close(session_pipe);
            }
            break;

        case 5:
            if (remove_box(session_pipe, buffer, args->head, op_code) == -1) {
                fprintf(stderr,"Unable to remove Box.\n");
                close(session_pipe);
            }
            break;

        case 7:
            if (list_box(session_pipe, args->head) == -1) {
                fprintf(stderr,"Unable to list boxes.\n");
                close(session_pipe);
            }
            break;

        default:
            fprintf(stderr,"Unknown OP_CODE given.\n");
        }

        buffer -= (UINT8_T_SIZE + PIPE_NAME_LENGTH);
        free(buffer);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr,"Instead of 3 arguments, %d were passed.\n", argc);
        return -1;
    }

    // Start TFS
    if (tfs_init(NULL) != 0) {
        fprintf(stderr,"Unable to start TFS.\n");
        return -1;
    }

    // Server's Pipe
    char *server_pipe_name = calloc(PIPE_NAME_LENGTH, sizeof(char));
    memcpy(server_pipe_name, PIPE_PATH, strlen(PIPE_PATH));
    memcpy(server_pipe_name + strlen(PIPE_PATH), argv[1],
           PIPE_NAME_LENGTH - strlen(PIPE_PATH));

    char *c;
    long max_sessions = strtol(argv[2], &c, 10);
    if (errno != 0 || *c != '\0' || max_sessions > INT_MAX ||
        max_sessions < INT_MIN) {
        fprintf(stderr,"Invalid Max Sessions value.\n");
        tfs_destroy();
        return -1;
    }

    if (unlink(server_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stderr,"Unlink(%s) failed: %s\n", server_pipe_name, strerror(errno));
        tfs_destroy();
        return -1;
    }

    if (mkfifo(server_pipe_name, 0777) != 0) {
        fprintf(stderr,"Unable to create Server's Pipe.\n");
        tfs_destroy();
        return -1;
    }

    // Linked list to store all the Boxes that are created
    struct Box *head = NULL;

    pc_queue_t *queue = malloc(sizeof(pc_queue_t));
    if (queue == NULL) {
        fprintf(stderr,"Unable to alloc for queue.\n");
        tfs_destroy();
        unlink(server_pipe_name);
        return -1;
    }
    pcq_create(queue, QUEUE_CAPACITY);

    // Create Worker Threads for each Session
    pthread_t sessions_tid[max_sessions];
    thread_args *args = malloc(sizeof(thread_args));
    if (args == NULL) {
        fprintf(stderr,"Unable to alloc for thread args.\n");
        tfs_destroy();
        unlink(server_pipe_name);
        pcq_destroy(queue);
        free(queue);
        return -1;
    }
    args->queue = queue;
    args->head = head;
    for (int i = 0; i < max_sessions; i++) {
        if (pthread_create(&sessions_tid[i], NULL, working_thread, args) != 0) {
            fprintf(stderr,"Error creating Thread(%d)\n", i);
            tfs_destroy();
            unlink(server_pipe_name);
            destroy_list(head);
            pcq_destroy(queue);
            free(queue);
            return -1;
        }
    }

    int server_pipe = open(server_pipe_name, O_RDONLY);
    if (server_pipe == -1) {
        fprintf(stderr,"Unable to open Server's Pipe.\n");
        tfs_destroy();
        unlink(server_pipe_name);
        destroy_list(head);
        pcq_destroy(queue);
        free(queue);
        return -1;
    }
    int dummy_server_pipe = open(server_pipe_name, O_WRONLY);
    if (dummy_server_pipe == -1) {
        fprintf(stderr,"Unable to open Server's Pipe.\n");
        close(server_pipe);
        tfs_destroy();
        unlink(server_pipe_name);
        pcq_destroy(queue);
        destroy_list(head);
        free(queue);
        return -1;
    }

    int run = TRUE;
    void *message = calloc(REQUEST_LENGTH, sizeof(char));
    while (run) {
        if (read(server_pipe, message, REQUEST_LENGTH) == -1) {
            fprintf(stderr,"Unable to read message to Server Pipe.\n");
            tfs_destroy();
            close(server_pipe);
            unlink(server_pipe_name);
            pcq_destroy(queue);
            destroy_list(head);
            free(message);
            free(queue);
            return -1;
        }

        if (pcq_enqueue(queue, message) == -1) {
            fprintf(stderr,"Unable to queue request.\n");
            tfs_destroy();
            close(server_pipe);
            unlink(server_pipe_name);
            pcq_destroy(queue);
            destroy_list(head);
            free(message);
            free(queue);
            return -1;
        }

        memset(message, 0, REQUEST_LENGTH);
    }

    free(message);

    for (int i = 0; i < max_sessions; i++) {
        if (pthread_join(sessions_tid[i], NULL) != 0) {
            fprintf(stderr,"Error creating Thread(%d)\n", i);
            tfs_destroy();
            close(server_pipe);
            unlink(server_pipe_name);
            pcq_destroy(queue);
            destroy_list(head);
            free(queue);
            return -1;
        }
    }

    pcq_destroy(queue);
    destroy_list(head);
    free(queue);

    if (close(server_pipe) == -1) {
        fprintf(stderr,"Error closing Server's Pipe.\n");
        tfs_destroy();
        unlink(server_pipe_name);
        return -1;
    }

    if (unlink(server_pipe_name) == -1) {
        fprintf(stderr,"Unlink(%s) failed: %s\n", server_pipe_name, strerror(errno));
        tfs_destroy();
        return -1;
    }

    if (tfs_destroy() != 0) {
        fprintf(stderr,"Error destroying TFS.\n");
        return -1;
    }

    return 0;
}