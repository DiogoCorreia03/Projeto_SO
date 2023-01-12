#ifndef __UTILS_COMMON_H__
#define __UTILS_COMMON_H__

#define PIPE_NAME_LENGTH (256)
#define BOX_NAME_LENGTH (32)
#define REGISTER_LENGTH (289)
#define TOTAL_RESPONSE_LENGTH (1029)
#define ERROR_MESSAGE_SIZE (1024)
#define TRUE (0)
#define FALSE (1)

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

#endif // __UTILS_COMMON_H__