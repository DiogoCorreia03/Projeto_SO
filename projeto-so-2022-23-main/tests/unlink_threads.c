#include "../fs/operations.h"
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

#define THREAD_COUNT 15
#define MAX_LEN_FILE_NAME 10

void *unlink_hardlink(void *arg);
void *unlink_symlink(void *arg);

int main() {

    // test to check if linking / sym linking is thread safe

    pthread_t tid[THREAD_COUNT];
    assert(tfs_init(NULL) != -1);
    int table[THREAD_COUNT];
    table[0] = 0;

    char target[MAX_FILE_NAME] = {"/f1"};
    int f = tfs_open(target, TFS_O_CREAT);
    assert(f != -1);

    assert(tfs_close(f) != -1);

    for (int i = 1; i < THREAD_COUNT; ++i) {
        switch (i % 2) {
        case 0:
            pthread_create(&tid[i], NULL, unlink_hardlink, &table[i]);
            break;

        case 1:
            pthread_create(&tid[i], NULL, unlink_symlink, &table[i]);
            break;

        default:
            break;
        }
    }

    for (int i = 1; i < THREAD_COUNT; ++i) {
        pthread_join(tid[i], NULL);
    }
    printf("Successful test\n");

    return 0;
}

void *unlink_hardlink(void *arg) {
    int file_i = *((int *)arg);

    char target[MAX_FILE_NAME] = {"/f1"};

    char link_name[MAX_FILE_NAME] = {"/l"};
    sprintf(link_name + 2, "%d", file_i);

    assert(tfs_link(target, link_name) != -1);

    return NULL;
}

void *unlink_symlink(void *arg) {
    int file_i = *((int *)arg);

    char target[MAX_FILE_NAME] = {"/f1"};

    char link_name[MAX_FILE_NAME] = {"/l"};
    sprintf(link_name + 2, "%d", file_i);

    assert(tfs_sym_link(target, link_name) != -1);

    return NULL;
}