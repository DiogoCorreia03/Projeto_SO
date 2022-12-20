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

#define THREAD_COUNT 17
#define MAX_LEN_FILE_NAME 10

void *create_file(void *arg);
void *close_file(void *arg);

int main() {

    // test to see if opening and closing different files is thread safe

    pthread_t tid[THREAD_COUNT];
    assert(tfs_init(NULL) != -1);
    int table[THREAD_COUNT];
    table[0] = 0;

    for (int i = 1; i < THREAD_COUNT; ++i) {
        table[i] = i;
        if (pthread_create(&tid[i], NULL, create_file, &table[i]) != 0)
            exit(1);
    }

    for (int i = 1; i < THREAD_COUNT; ++i) {
        pthread_join(tid[i], NULL);
    }

    for (int i = 1; i < THREAD_COUNT; ++i) {
        if (pthread_create(&tid[i], NULL, close_file, &table[i]) != 0)
            exit(1);
    }

    for (int i = 1; i < THREAD_COUNT; ++i) {
        pthread_join(tid[i], NULL);
    }

    printf("Successful test.\n");

    return 0;
}

void *create_file(void *arg) {
    int file_i = *((int *)arg);

    char path[MAX_LEN_FILE_NAME] = {"/f"};
    sprintf(path + 2, "%d", file_i);

    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    assert(tfs_write(f, path, strlen(path) + 1) == strlen(path) + 1);

    return NULL;
}

void *close_file(void *arg) {
    int file_i = *((int *)arg);

    if (file_i == 0) {
        assert(tfs_destroy() != -1);

    } else {
        sleep(1);
        assert(tfs_close(file_i - 1) == 0);
    }

    return NULL;
}