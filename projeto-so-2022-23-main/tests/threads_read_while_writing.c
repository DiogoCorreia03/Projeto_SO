#include "../fs/operations.h"
#include <assert.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void *write_file(void *file_path){
    const char write_contents1[] = "Hello World!1";
    const char write_contents2[] = "Hello World!2";
    const char write_contents3[] = "Hello World!3";
    const char write_contents4[] = "Hello World!4";
    const char write_contents5[] = "Hello World!5";
    const char **write_contents = malloc(sizeof(char*) * 5);
    write_contents[0] = write_contents1;
    write_contents[1] = write_contents2;
    write_contents[2] = write_contents3;
    write_contents[3] = write_contents4;
    write_contents[4] = write_contents5;
    for (int j = 0; j < 100; j++){
        for (int i = 0; i < 5; i++){
            int fd = tfs_open((char*)file_path, TFS_O_CREAT);
            assert(fd!=-1);
            assert(tfs_write(fd, write_contents[i], 14) != -1);
            tfs_close(fd);
        }
    }
    return 0;
}

void *read_file(void *file_path){

    const char write_contents1[] = "Hello World!1";
    const char write_contents2[] = "Hello World!2";
    const char write_contents3[] = "Hello World!3";
    const char write_contents4[] = "Hello World!4";
    const char write_contents5[] = "Hello World!5";
    const char **write_contents = malloc(sizeof(char*) * 5);
    write_contents[0] = write_contents1;
    write_contents[1] = write_contents2;
    write_contents[2] = write_contents3;
    write_contents[3] = write_contents4;
    write_contents[4] = write_contents5;
    
    unsigned long len = sizeof("Hello World!5");
    char buffer[len];
    for (int i = 0; i < 500; i++){
        int fhandle = tfs_open(file_path, TFS_O_CREAT);
        assert(fhandle != -1);
        assert(tfs_read(fhandle, buffer, 14)!= -1);
        assert(memcmp(write_contents[0], buffer, sizeof(char) * 14) == 0 ||
            memcmp(write_contents[1], buffer, sizeof(char) * 14) == 0||
            memcmp(write_contents[2], buffer, sizeof(char) * 14) == 0||
            memcmp(write_contents[3], buffer, sizeof(char) * 14) == 0||
            memcmp(write_contents[4], buffer, sizeof(char) * 14) == 0);
        tfs_close(fhandle);
    }
    return 0;
}


int main() {
    const char *file_path = "/f1";

    assert(tfs_init(NULL) != -1);

    
    // Write to file
    pthread_t t1, t2;
    int val1 = pthread_create(&t1, NULL ,&write_file, (void*)file_path);
    assert(val1 == 0);
    int val2 = pthread_create(&t2, NULL, &read_file, (void*)file_path);
    assert(val2 == 0);

    val1 = pthread_join(t1, NULL);
    assert(val1 == 0);

    val2 = pthread_join(t2,NULL);
    assert(val2 == 0);



    printf("Successful test.\n");
}
