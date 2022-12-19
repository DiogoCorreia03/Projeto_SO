#include "fs/operations.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define THREAD_COUNT 16
#define MAX_LEN_FILE_NAME 30

void *read_file(void *arg);
void *copy_from_external0(void *arg);
void *copy_from_external1(void *arg);

int main() {

    pthread_t tid[THREAD_COUNT];
    pthread_t tid2[THREAD_COUNT];
    assert(tfs_init(NULL) != -1);
    int table[THREAD_COUNT];
    table[0] = 0;
    int table2[THREAD_COUNT];
    table2[0] = 0;

    for (int i = 1; i < THREAD_COUNT; ++i) {
        table[i] = i;
        switch (i % 2) {

        case 0:
            pthread_create(&tid[i], NULL, copy_from_external0, &table[i]);
            break;

        case 1:
            pthread_create(&tid[i], NULL, copy_from_external1, &table[i]);
            break;

        default:
            break;
        }
    }

    for (int i = 1; i < THREAD_COUNT; ++i) {
        pthread_join(tid[i], NULL);
    }

    char buffer[1200];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, "BBB BBB", 7);
    char buffer2[1200];
    memset(buffer2, 0, sizeof(buffer2));

    int fhandle = tfs_open("/l", TFS_O_CREAT);
    ssize_t read = tfs_read(fhandle, buffer2, sizeof(buffer2) - 1);

    assert(read == strlen(buffer));
    assert(!memcmp(buffer, buffer2, strlen(buffer2)));

    tfs_close(fhandle);

    for (int i = 1; i < THREAD_COUNT; ++i) {
        table2[i] = i;
        switch (i % 2) {
        case 1:
            pthread_create(&tid2[i], NULL, read_file, &table2[i]);
            break;
        default:
            break;
        }
    }

    for (int i = 1; i < THREAD_COUNT; ++i) {
        switch (i % 2) {
        case 1:
            pthread_join(tid2[i], NULL);
            break;
        default:
            break;
        }
    }

    printf("Successful test\n");

    return 0;
}

void *read_file(void *arg) {
    int file_i = *((int *)arg);

    char path[MAX_LEN_FILE_NAME] = {"/f"};
    sprintf(path + 2, "%d", file_i);

    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    char buffer3[1200];
    memset(buffer3, 0, sizeof(buffer3));
    char buffer4[1200];
    memset(buffer4, 0, sizeof(buffer4));
    memcpy(buffer4, "BBB BBB", 8);

    assert(tfs_read(f, buffer3, sizeof(buffer3) - 1) == strlen(buffer4));
    assert(!memcmp(buffer3, buffer4, strlen(buffer3)));

    tfs_close(f);

    return NULL;
}

void *copy_from_external0(void *arg) {
    (void)arg;

    char source[MAX_LEN_FILE_NAME] = {"tests/threads_external.txt"};

    char dest[MAX_LEN_FILE_NAME] = {"/l"};

    assert(tfs_copy_from_external_fs(source, dest) != -1);

    return NULL;
}

void *copy_from_external1(void *arg) {
    int file_i = *((int *)arg);

    char source[MAX_LEN_FILE_NAME] = {"tests/threads_external.txt"};

    char dest[MAX_LEN_FILE_NAME] = {"/f"};
    sprintf(dest + 2, "%d", file_i);

    assert(tfs_copy_from_external_fs(source, dest) != -1);

    return NULL;
}