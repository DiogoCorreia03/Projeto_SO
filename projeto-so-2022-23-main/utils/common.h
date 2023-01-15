#ifndef __UTILS_COMMON_H__
#define __UTILS_COMMON_H__

#include "../fs/operations.h"
#include "../producer-consumer/producer-consumer.h"
#include <pthread.h>
#include <stdint.h>

#define PIPE_NAME_LENGTH (256 * sizeof(char))
#define BOX_NAME_LENGTH (32 * sizeof(char))
#define UINT8_T_SIZE (sizeof(uint8_t))
#define REQUEST_LENGTH (PIPE_NAME_LENGTH + BOX_NAME_LENGTH + UINT8_T_SIZE)
#define TOTAL_RESPONSE_LENGTH (1029)
#define ERROR_MESSAGE_SIZE (1024)
#define TRUE (1)
#define FALSE (0)
#define LIST_REQUEST (257)
#define LIST_RESPONSE (58)
#define QUEUE_CAPACITY (200)
#define MESSAGE_SIZE (1024)

static const uint8_t PUB_REGISTER = 1;
static const uint8_t SUB_REGISTER = 2;
static const uint8_t BOX_CREATION_R = 3;
static const uint8_t BOX_CREATION_A = 4;
static const uint8_t BOX_REMOVAL_R = 5;
static const uint8_t BOX_REMOVAL_A = 6;
static const uint8_t LIST_BOX_R = 7;
static const uint8_t LIST_BOX_A = 8;
static const uint8_t PUB_2_SERVER = 9;
static const uint8_t SERVER_2_SUB = 10;
static const int32_t BOX_SUCCESS = 0;
static const int32_t BOX_ERROR = -1;
static const uint8_t LAST_BOX = 1;
static const char PIPE_PATH[] = "../tmp/";

extern pthread_mutex_t sub_lock;
extern pthread_cond_t sub_cond;

typedef struct {
    int session_pipe;
    char box_name[BOX_NAME_LENGTH];
} Client_Info;

struct Box {
    char box_name[BOX_NAME_LENGTH];
    uint64_t box_size;
    uint64_t n_publishers;
    uint8_t last;
    uint64_t n_subscribers;
    struct Box *next;
};

typedef struct {
    pc_queue_t *queue;
    struct Box *head;
} thread_args;

struct Box *getBox(struct Box *head, char *box_name);

int insertBox(struct Box *head, char *box_name, uint64_t box_size);

int insertionSort(struct Box *head, char *box_name, uint64_t box_size,
                  uint64_t n_publishers, uint64_t n_subscribers);

int deleteBox(struct Box *head, char *box_name);

void box_to_string(struct Box *box, char *buffer);

void destroy_list(struct Box *head);

void print_list(struct Box *head);

#endif // __UTILS_COMMON_H__
