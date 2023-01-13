#ifndef __UTILS_COMMON_H__
#define __UTILS_COMMON_H__

#include <stdint.h>
#include "../fs/operations.h"

#define PIPE_NAME_LENGTH (256 * sizeof(char))
#define BOX_NAME_LENGTH (32 * sizeof(char))
#define UINT8_T_SIZE (sizeof(uint8_t))
#define REGISTER_LENGTH (PIPE_NAME_LENGTH + BOX_NAME_LENGTH + UINT8_T_SIZE) // FIXME
#define TOTAL_RESPONSE_LENGTH (1029)
#define ERROR_MESSAGE_SIZE (1024)
#define BLOCK_SIZE file_size()
#define TRUE (1)
#define FALSE (0)
#define LIST_REQUEST (257)
#define LIST_RESPONSE (58)

const uint8_t PUB_REGISTER = 1;
const uint8_t SUB_REGISTER = 2;
const uint8_t BOX_CREATION_R = 3;
const uint8_t BOX_CREATION_A = 4;
const uint8_t BOX_REMOVAL_R = 5;
const uint8_t BOX_REMOVAL_A = 6;
const uint8_t LIST_BOX_R = 7;
const uint8_t LIST_BOX_A = 8;
const uint8_t PUB_2_SERVER = 9;
const uint8_t SERVER_2_SUB = 10;

typedef struct{
    char box_name[BOX_NAME_LENGTH];
    int file_handle;
    uint64_t box_size;
    uint64_t n_publishers;
    uint64_t n_subscribers;
    struct Box *next;
} Box;

Box* getBox(struct Box *head, char *box_name);

int insertBox(struct Box *head, char *box_name, int file_handle, uint64_t box_size);

#endif // __UTILS_COMMON_H__

