#include "../fs/operations.h"
#include <assert.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


//apagar
void *write_file(void *file_path){
    int fd = tfs_open((char*)file_path, TFS_O_CREAT);
    const char write_contents1[] = "Hello World!1";
    assert(fd!=-1);
    assert(tfs_write(fd, write_contents1, 14) != -1);
    tfs_close(fd);
    return 0;
}

void *open_file(void *file_path){
    sleep(1);
    int fhandle = tfs_open(file_path, TFS_O_APPEND);
    assert(tfs_unlink(file_path) == -1);
    //assert(tfs_close(fhandle)== 0);
    assert(fhandle != -1);
    return 0;
}

void *unlink_file(void *file_path){
    sleep(2);
    assert(tfs_unlink(file_path) == -1);
    return 0;
}

//void *close_file(void *file_path){
  //  assert(tfs_close(file_path) == 0);
  //  return 0;
//}


int main() {
    const char *file_path = "/f1";

    assert(tfs_init(NULL) != -1);

    
    // Write to file
    pthread_t t1, t2, t3;
    int val1 = pthread_create(&t1, NULL ,&write_file, (void*)file_path);
    assert(val1 == 0);
    int val2 = pthread_create(&t2, NULL, &open_file, (void*)file_path);
    assert(val2 == 0);
    int val3 = pthread_create(&t3, NULL ,&unlink_file, (void*)file_path);
    assert(val3 == 0);

    val1 = pthread_join(t1, NULL);
    assert(val1 == 0);

    val2 = pthread_join(t2,NULL);
    assert(val2 == 0);

    val3 = pthread_join(t3, NULL);
    assert(val3 == 0);



    printf("Successful test.\n");
}
