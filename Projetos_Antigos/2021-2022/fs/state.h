#ifndef STATE_H
#define STATE_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>

/* tfs_open flags */
enum {
    TFS_O_CREAT = 0b001,
    TFS_O_TRUNC = 0b010,
    TFS_O_APPEND = 0b100,
};

/*
 * Directory entry
 */
typedef struct {
    char d_name[MAX_FILE_NAME];
    int d_inumber;
} dir_entry_t;

typedef enum { T_FILE, T_DIRECTORY } inode_type;

/*
 * I-node
 */
typedef struct {
    inode_type i_node_type;
    size_t i_size;
    int i_data_blocks[11];
    /* in a real FS, more fields would exist here */
} inode_t;

typedef enum { FREE = 0, TAKEN = 1 } allocation_state_t;

/*
 * Open file entry (in open file table)
 */
typedef struct {
    int of_inumber;
    size_t of_offset;
} open_file_entry_t;

#define MAX_DIR_ENTRIES (BLOCK_SIZE / sizeof(dir_entry_t))

void state_init();
void state_destroy();
void state_destroy_after_all_closed();

int inode_create(inode_type n_type);
int inode_empty_content(int inumber);
int inode_delete(int inumber);
inode_t *inode_get(int inumber);

int clear_dir_entry(int inumber, int sub_inumber);
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name);
int find_in_dir(int inumber, char const *sub_name);

int data_block_alloc();
int data_block_free(int block_number);
void *data_block_get(int block_number);

int add_to_open_file_table(int inumber, size_t offset);
int remove_from_open_file_table(int fhandle);
open_file_entry_t *get_open_file_entry(int fhandle);

int file_create(char const *name);
int file_open(int inum, char const *name, int flags);
ssize_t file_write_content(int fhandle, void const *buffer, size_t len);
ssize_t file_read_content(int fhandle, void *buffer, size_t to_read);

void file_open_lock();
void file_open_unlock();

#endif // STATE_H
