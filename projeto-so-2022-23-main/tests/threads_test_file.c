#include "operations.h"
#include "config.h"
#include "state.h"
#include "betterassert.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>


void* tfs_write_thread(void* args){ //args = ponteiro void para o array de ponteiros void
        //        args = void* para void*[] = void** para posicao 0
    void** coisas = (void**)args;

    int* fhandle = coisas[0]; //void*
    //printf("fhandle: %d\n", *fhandle);
    size_t* len = coisas[2];
    //printf("len: %ld\n", *len);
    void* buffer = coisas[1];
    //printf("buffer: %s\n", (char*) buffer);
    //printf("write: %ld\n",tfs_write(*fhandle, buffer , *len));
    tfs_write(*fhandle, buffer , *len);
    return 0;
}


void* tfs_read_thread(void* args){ //args = ponteiro void para o array de ponteiros void
        //        args = void* para void*[] = void** para posicao 0

    void** coisas = (void**)args;

    int* fhandle = coisas[0]; //void*
    //printf("fhandle: %d\n", *fhandle);
    size_t* len = coisas[2];
    //printf("len: %ld\n", *len);
    void* buffer = coisas[1];
    //printf("buffer: %s\n", (char*) buffer);
    //printf("read: %ld\n",tfs_read(*fhandle, buffer , *len));
    tfs_read(*fhandle, buffer , *len);
    return 0;
}


void* tfs_close_thread(void* args){ //args = ponteiro void para o array de ponteiros void
        //        args = void* para void*[] = void** para posicao 0

    int* fhandle_p = args;
    printf("close: %d\n", tfs_close(*fhandle_p));
    return 0;
}



int main(int argc, char* const argv[])
{
    tfs_init(NULL);
    (void)argc;
    (void)argv;
    

    /*
    int fhandle = 3;
    size_t len = 20;
    char buffer1[] = "ola";
    void* cenas[] = {(&fhandle), (buffer1), (&len)}; // ponteiro para array de ponteiros
    tfs_write_thread(cenas);
    */
    //ssize_t (*func_ptr[2])(int fhandle,  void * buffer , size_t to_write) = {tfs_read, tfs_write};

    //srand ( time(NULL) );    int randomIndex

    char source_file_name[] = "tests/f9.txt";
    char new_file_name[] = "/f1";
    int new_file_fhandle;
    char* buffer = "AAA!";
    char buffer_read[1024];
    memset(buffer_read, 0, sizeof(buffer_read));
    //memset(buffer_read, 0, strlen());
    if (tfs_copy_from_external_fs(source_file_name, new_file_name) == -1) return -1;
    new_file_fhandle = tfs_open(new_file_name, TFS_O_CREAT);
    //printf("New File Content: %s\n", buffer);

    size_t len2 = strlen(buffer);
    size_t len1= strlen(buffer);
    //int new_file_fhandle1;
    void* cenas1[] ={&new_file_fhandle, buffer_read, &len2};
    void* cenas2[] ={&new_file_fhandle, buffer, &len1};
    /*
    tfs_read_thread(cenas1);
    printf("New File Content: %s\n", buffer);
    */

    //srand ( time(NULL) );
    //int myArray[3] = { 1,2,3};
    //int randomIndex = rand() % 3;
    //int randomValue = myArray[randomIndex];
   
    
    pthread_t thread_id[1000];


    tfs_read_thread(cenas1);
    for(int i = 0; i < 1000; i++){ 
        //printf("i: %d\n", i);
        switch(i%2){
            case 5: 
                printf("here close{%d}\n", i);
                pthread_create(&thread_id[i], NULL,tfs_close_thread , (&new_file_fhandle));
                break;
            case 1: 
                //printf("here read{%d}\n", i);
                pthread_create(&thread_id[i], NULL, tfs_write_thread , cenas2);
                //printf("New File Content: %s\n", buffer);
                break;
            case 0: 
                //new_file_fhandle1 = tfs_open(new_file_name, TFS_O_CREAT);
                //tfs_close(new_file_fhandle);
                //new_file_fhandle = tfs_open(new_file_name, TFS_O_CREAT);
                pthread_create(&thread_id[i], NULL, tfs_read_thread , cenas1);
                //tfs_close(new_file_fhandle);
                //tfs_close(new_file_fhandle1);
                //printf("New File Content: %s\n", buffer);
                break;
            default: 
                break;
        }

    }
    //printf("DONE1\n");
    
    for(int i = 0; i < 1000; i++){ 
        pthread_join(thread_id[i], NULL);    
    }   

    //printf("DONE2\n");
    tfs_close(new_file_fhandle);
    //printf("DONE3\n");
    int file_fhandle = tfs_open(new_file_name, TFS_O_CREAT);
    //printf("DONE4\n");
    char buffer3[1025];
    memset(buffer3,0,sizeof(buffer3));
    printf("read depois: %ld\n",tfs_read(file_fhandle, buffer3, sizeof(buffer3)));
    //printf("DONE5\n");
    printf("buffer: %s\n", buffer3);
    //printf("DONE6\n");
    tfs_close(file_fhandle);
    //printf("DONE7\n");
    printf("Successful test.\n");

    return 0;
}