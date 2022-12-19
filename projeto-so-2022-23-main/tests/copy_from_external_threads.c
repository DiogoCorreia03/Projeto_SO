#include "fs/operations.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>


#define THREAD_COUNT 101
#define MAX_LEN_FILE_NAME 30

void *create_file(void *arg);
void *copy_from_external0(void *arg);
void *copy_from_external1(void *arg);
void *close_file(void *arg);

int main() {

    pthread_t tid[THREAD_COUNT];
    assert(tfs_init(NULL) != -1);
    int table[THREAD_COUNT];
    table[0] = 0;

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

    char buffer[1200]; // = "BBB BBB";
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, "BBB BBB", 7);
    char buffer2[1200];
    memset(buffer2, 0, sizeof(buffer2));

    int fhandle = tfs_open("/l", TFS_O_CREAT);

    assert(tfs_read(fhandle, buffer2, strlen(buffer2)) == strlen(buffer));
    assert(!memcmp(buffer, buffer2, strlen(buffer2)));

    /*
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

    */

    printf("Successful test\n");

    return 0;
}

void *create_file(void *arg) {
    int file_i = *((int *)arg);

    char path[MAX_LEN_FILE_NAME] = {"/f"};
    sprintf(path + 2, "%d", file_i);

    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    char *content = "ABCDEFG";

    assert(tfs_write(f, content, strlen(content)) == strlen(content));

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