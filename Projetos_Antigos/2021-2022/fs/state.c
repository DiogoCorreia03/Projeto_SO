#include "state.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* Persistent FS state  (in reality, it should be maintained in secondary
 * memory; for simplicity, this project maintains it in primary memory) */

/* I-node table */
static inode_t inode_table[INODE_TABLE_SIZE];
static allocation_state_t freeinode_ts[INODE_TABLE_SIZE];

/* Data blocks */
static char fs_data[BLOCK_SIZE * DATA_BLOCKS];
static allocation_state_t free_blocks[DATA_BLOCKS];

/* Volatile FS state */
static open_file_entry_t open_file_table[MAX_OPEN_FILES];
static allocation_state_t free_open_file_entries[MAX_OPEN_FILES];
static int open_files;

static bool destroying;

static inline bool valid_inumber(int inumber) {
    return inumber >= 0 && inumber < INODE_TABLE_SIZE;
}

static inline bool valid_block_number(int block_number) {
    return block_number >= 0 && block_number < DATA_BLOCKS;
}

static inline bool valid_file_handle(int file_handle) {
    return file_handle >= 0 && file_handle < MAX_OPEN_FILES;
}

static pthread_mutex_t file_lock[MAX_OPEN_FILES];
static pthread_rwlock_t inode_lock[INODE_TABLE_SIZE];
static pthread_mutex_t file_table_lock;
static pthread_mutex_t inode_creation_lock;
static pthread_mutex_t data_block_lock;
static pthread_mutex_t fo_lock;

static pthread_mutex_t of_number_lock;

static pthread_cond_t no_open_files;

/**
 * We need to defeat the optimizer for the insert_delay() function.
 * Under optimization, the empty loop would be completely optimized away.
 * This function tells the compiler that the assembly code being run (which is
 * none) might potentially change *all memory in the process*.
 *
 * This prevents the optimizer from optimizing this code away, because it does
 * not know what it does and it may have side effects.
 *
 * Reference with more information: https://youtu.be/nXaxk27zwlk?t=2775
 *
 * Exercise: try removing this function and look at the assembly generated to
 * compare.
 */
static void touch_all_memory() { __asm volatile("" : : : "memory"); }

/*
 * Auxiliary function to insert a delay.
 * Used in accesses to persistent FS state as a way of emulating access
 * latencies as if such data structures were really stored in secondary memory.
 */
static void insert_delay() {
    for (int i = 0; i < DELAY; i++) {
        touch_all_memory();
    }
}

/*
 * Initializes FS state
 */
void state_init() {
    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        freeinode_ts[i] = FREE;
		pthread_rwlock_init(inode_lock+i, NULL);
    }
	pthread_mutex_init(&inode_creation_lock, NULL);

    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        free_blocks[i] = FREE;
    }
	pthread_mutex_init(&data_block_lock, NULL);

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        free_open_file_entries[i] = FREE;
		pthread_mutex_init(file_lock+i, NULL);
    }
	pthread_mutex_init(&fo_lock, NULL);

	destroying = false;
	open_files = 0;
}

void state_destroy() { 
	/* Destroy locks */
	for(int i=0; i<MAX_OPEN_FILES; i++){
		pthread_mutex_destroy(file_lock+i);
	}
	pthread_mutex_destroy(&file_table_lock);
	for(int i=0; i<INODE_TABLE_SIZE; i++){
		pthread_rwlock_destroy(inode_lock+i);
	}
	pthread_mutex_destroy(&inode_creation_lock);
	pthread_mutex_destroy(&data_block_lock);
	pthread_mutex_destroy(&fo_lock);
}

void state_destroy_after_all_closed(){
	// TODO: verificar locks
	destroying = true;
	pthread_mutex_lock(&file_table_lock);
	while(open_files > 0){
		pthread_cond_wait(&no_open_files, &file_table_lock);
	}
	pthread_mutex_unlock(&file_table_lock);
	state_destroy();
}

/*
 * Creates a new i-node in the i-node table.
 * Input:
 *  - n_type: the type of the node (file or directory)
 * Returns:
 *  new i-node's number if successfully created, -1 otherwise
 */
int inode_create(inode_type n_type) {
	/* Only one thread shall search for a free entry at a time */
	pthread_mutex_lock(&inode_creation_lock);
    for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++) {
        if ((inumber * (int) sizeof(allocation_state_t) % BLOCK_SIZE) == 0) {
            insert_delay(); // simulate storage access delay (to freeinode_ts)
        }

        /* Finds first free entry in i-node table */
        if (freeinode_ts[inumber] == FREE) {
			/* Nobody change this inode */
			pthread_rwlock_wrlock(inode_lock+inumber);
			
            /* Found a free entry, so takes it for the new i-node*/
            freeinode_ts[inumber] = TAKEN;

			/* No longer searching for a place for the new inode */
			pthread_mutex_unlock(&inode_creation_lock);

            insert_delay(); // simulate storage access delay (to i-node)
            inode_table[inumber].i_node_type = n_type;

            if (n_type == T_DIRECTORY) {
                /* Initializes directory (filling its block with empty
                 * entries, labeled with inumber==-1) */
                int b = data_block_alloc();
                if (b == -1) {
                    freeinode_ts[inumber] = FREE;
					pthread_rwlock_unlock(inode_lock+inumber);
                    return -1;
                }

                inode_table[inumber].i_size = BLOCK_SIZE;
				memset(inode_table[inumber].i_data_blocks, -1, 11*sizeof(int));
				inode_table[inumber].i_data_blocks[0] = b;

                dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(b);
                if (dir_entry == NULL) {
                    freeinode_ts[inumber] = FREE;
					pthread_rwlock_unlock(inode_lock+inumber);
                    return -1;
                }

                for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
                    dir_entry[i].d_inumber = -1;
                }
            } else {
                /* In case of a new file, simply sets its size to 0 */
                inode_table[inumber].i_size = 0;
				memset(inode_table[inumber].i_data_blocks, -1, 11*sizeof(int));
            }
			pthread_rwlock_unlock(inode_lock+inumber);
            return inumber;
        }
    }
	pthread_mutex_unlock(&inode_creation_lock);
    return -1;
}

/*
 * Deletes the content of the i-node (frees the data blocks)
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
/* NOTE: The data changed by this function shall be locked from the outside */
int inode_empty_content(int inumber){
	inode_t inode = inode_table[inumber];
	size_t i_block_number = inode.i_size / BLOCK_SIZE;
    if (inode.i_size > 0) {
		for(size_t i=0; i<i_block_number && i<10; i++){
			if(data_block_free(inode.i_data_blocks[i]) == -1){
				return -1;
			}
		}
    }
	if(inode.i_data_blocks[10] != -1){
		int * block = (int*)data_block_get(inode.i_data_blocks[10]);
		for(size_t i=0; i<i_block_number-10; i++){
			if(data_block_free(block[i]) == -1){
				return -1;
			}
		}
		if(data_block_free(inode.i_data_blocks[10]) == -1){
			return -1;
		}
	}
	return 0;
}

/*
 * Deletes the i-node.
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_delete(int inumber) {
	/* Nobody change this inode while deleting */
    insert_delay();
    insert_delay();

    if (!valid_inumber(inumber) || freeinode_ts[inumber] == FREE) {
        return -1;
    }

	pthread_rwlock_wrlock(inode_lock+inumber);
	/* Free the blocks allocated by the inode */
	if(inode_empty_content(inumber) == -1){
		pthread_rwlock_unlock(inode_lock+inumber);
		return -1;
	}

    freeinode_ts[inumber] = FREE;
	pthread_rwlock_unlock(inode_lock+inumber);
    return 0;
}

/*
 * Returns a pointer to an existing i-node.
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: pointer if successful, NULL if failed
 */
inode_t *inode_get(int inumber) {
    if (!valid_inumber(inumber)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to i-node
    return &inode_table[inumber];
}

/*
 * Adds an entry to the i-node directory data.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 *  - sub_name: name of the sub i-node entry
 * Returns: SUCCESS or FAIL
 */
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name) {
    if (!valid_inumber(inumber) || !valid_inumber(sub_inumber)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to i-node with inumber

	/* Changing this directory, do not interfere */
	pthread_rwlock_wrlock(inode_lock+inumber);
    if (inode_table[inumber].i_node_type != T_DIRECTORY) {
		pthread_rwlock_unlock(inode_lock+inumber);
        return -1;
    }

    if (strlen(sub_name) == 0) {
		pthread_rwlock_unlock(inode_lock+inumber);
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode_table[inumber].i_data_blocks[0]);
    if (dir_entry == NULL) {
		pthread_rwlock_unlock(inode_lock+inumber);
        return -1;
    }

    /* Finds and fills the first empty entry */
    for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
        if(dir_entry[i].d_inumber == -1){
            dir_entry[i].d_inumber = sub_inumber;
            strncpy(dir_entry[i].d_name, sub_name, MAX_FILE_NAME - 1);
            dir_entry[i].d_name[MAX_FILE_NAME - 1] = 0;

			pthread_rwlock_unlock(inode_lock+inumber);
            return 0;
        }
    }

	pthread_rwlock_unlock(inode_lock+inumber);
    return -1;
}

/* Looks for a given name inside a directory
 * Input:
 * 	- parent directory's i-node number
 * 	- name to search
 * 	Returns i-number linked to the target name, -1 if not found
 */
int find_in_dir(int inumber, char const *sub_name) {
    insert_delay(); // simulate storage access delay to i-node with inumber

	/* I'm reading, do not disturb me */
	pthread_rwlock_rdlock(inode_lock+inumber);

    if (!valid_inumber(inumber) ||
        inode_table[inumber].i_node_type != T_DIRECTORY) {
		pthread_rwlock_unlock(inode_lock+inumber);
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode_table[inumber].i_data_blocks[0]);
    if (dir_entry == NULL) {
		pthread_rwlock_unlock(inode_lock+inumber);
        return -1;
    }

    /* Iterates over the directory entries looking for one that has the target
     * name */
    for (int i = 0; i < MAX_DIR_ENTRIES; i++){
        if((dir_entry[i].d_inumber != -1) &&
          (strncmp(dir_entry[i].d_name, sub_name, MAX_FILE_NAME) == 0)){
			pthread_rwlock_unlock(inode_lock+inumber);
            return dir_entry[i].d_inumber;
        }
	}

	pthread_rwlock_unlock(inode_lock+inumber);
    return -1;
}
/* 
 * Note: allowing concurrent calls of find_in_dir on the same inumber would 
 * require locks on the data_block level. This would be very expensive, for 
 * example on a memory level (a lot of locks would be required). Therefore, 
 * only one call to this function shall run at a time, in this implementation. 
 */

/*
 * Allocated a new data block
 * Returns: block index if successful, -1 otherwise
 */
int data_block_alloc() {
    for (int i = 0; i < DATA_BLOCKS; i++) {
        if (i * (int) sizeof(allocation_state_t) % BLOCK_SIZE == 0) {
            insert_delay(); // simulate storage access delay to free_blocks
        }

		// Only one thread at a time shall alloc a data block
		pthread_mutex_lock(&data_block_lock);
        if (free_blocks[i] == FREE) {
            free_blocks[i] = TAKEN;
			pthread_mutex_unlock(&data_block_lock);
            return i;
        }
		pthread_mutex_unlock(&data_block_lock);
    }
    return -1;
}

/* Frees a data block
 * Input
 * 	- the block index
 * Returns: 0 if success, -1 otherwise
 */
int data_block_free(int block_number) {
    if (!valid_block_number(block_number)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to free_blocks
	pthread_mutex_lock(&data_block_lock);
    free_blocks[block_number] = FREE;
	pthread_mutex_unlock(&data_block_lock);
    return 0;
}

/* Returns a pointer to the contents of a given block
 * Input:
 * 	- Block's index
 * Returns: pointer to the first byte of the block, NULL otherwise
 */
void *data_block_get(int block_number) {
    if (!valid_block_number(block_number)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to block
    return &fs_data[block_number * BLOCK_SIZE];
}

/* Add new entry to the open file table
 * Inputs:
 * 	- I-node number of the file to open
 * 	- Initial offset
 * Returns: file handle if successful, -1 otherwise
 */
int add_to_open_file_table(int inumber, size_t offset) {
	pthread_mutex_lock(&file_table_lock);
    for(int i=0; i<MAX_OPEN_FILES; i++) {

        if(free_open_file_entries[i] == FREE){
            free_open_file_entries[i] = TAKEN;
            open_file_table[i].of_inumber = inumber;
            open_file_table[i].of_offset = offset;

			pthread_mutex_lock(&of_number_lock);
			open_files++;
			pthread_mutex_unlock(&of_number_lock);
			
			pthread_mutex_unlock(&file_table_lock);
            return i;
        }
    }
	pthread_mutex_unlock(&file_table_lock);
    return -1;
}

/* Frees an entry from the open file table
 * Inputs:
 * 	- file handle to free/close
 * Returns 0 is success, -1 otherwise
 */
int remove_from_open_file_table(int fhandle) {
	pthread_mutex_lock(&file_table_lock);
	/* Only one thread shall remove the file from the table */
	pthread_mutex_lock(file_lock+fhandle);

    if (!valid_file_handle(fhandle) ||
        free_open_file_entries[fhandle] != TAKEN) {
		pthread_mutex_unlock(file_lock+fhandle);
		pthread_mutex_unlock(&file_table_lock);
        return -1;
    }
    free_open_file_entries[fhandle] = FREE;

	pthread_mutex_lock(&of_number_lock);
	open_files--;
	pthread_mutex_unlock(&of_number_lock);

	if(open_files == 0 && destroying)
		pthread_cond_signal(&no_open_files);

	pthread_mutex_unlock(file_lock+fhandle);
	pthread_mutex_unlock(&file_table_lock);
    return 0;
}

/* Returns pointer to a given entry in the open file table
 * Inputs:
 * 	 - file handle
 * Returns: pointer to the entry if sucessful, NULL otherwise
 */
open_file_entry_t *get_open_file_entry(int fhandle) {
    if (!valid_file_handle(fhandle)) {
        return NULL;
    }
    return &open_file_table[fhandle];
}

/*
 * Returns:
 * - the index of the nth block with data relative to the inode on success
 * - -2 if the inode does not have that many blocks
 * - -1 on failure
 */
int get_nth_block(inode_t * inode, size_t n){
	/* Should be locked (for read, at least) from the outside */
	if(inode == NULL || n >= 10 + BLOCK_SIZE / sizeof(int))
		return -1;
	if(inode->i_size == 0 || n > (inode->i_size - 1) / BLOCK_SIZE)
		return -2;
	if(n < 10)
		return inode->i_data_blocks[n];
	int* reference_block = (int*)data_block_get(inode->i_data_blocks[10]);
	return reference_block[n-10];
}

/*
 * Allocs a new block for the inode, so that it has n blocks
 */
ssize_t inode_alloc_nth_block(inode_t *inode, size_t n){
	/* Should be locked (for write) from the outside */
	int block_number = -1;
	if(n < 10){
		block_number = data_block_alloc();
		if(block_number == -1) return -1;
		inode->i_data_blocks[n] = block_number;
	}else{
		if(n == 10){
			int irreference_block_number = data_block_alloc();
			if(irreference_block_number == -1) return -1;
			inode->i_data_blocks[10] = irreference_block_number;
		}
		int* irreference_block = (int*) data_block_get(inode->i_data_blocks[10]);
		if(irreference_block == NULL) return -1;
		int* new_block_position = irreference_block + n - 10;
		block_number = data_block_alloc();
		if(block_number == -1) return -1;
		*new_block_position = block_number;
	}
	return block_number;
}

int file_open(int inum, char const *name, int flags){
	if(destroying)
		return -1;
	size_t offset = 0;

	if(inum >= 0){
		pthread_rwlock_wrlock(inode_lock+inum);
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
			pthread_rwlock_unlock(inode_lock+inum);
            return -1;
        }

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
			if(inode_empty_content(inum) == -1){
				pthread_rwlock_unlock(inode_lock+inum);
				return -1;
			}
			inode->i_size = 0;
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
		pthread_rwlock_unlock(inode_lock+inum);
	}else if(flags & TFS_O_CREAT){
        /* The file doesn't exist; the flags specify that it should be created*/
		/* Create inode */
		inum = inode_create(T_FILE);
		if (inum == -1) {
			return -1;
		}
		pthread_rwlock_wrlock(inode_lock+inum);
		/* Add entry in the root directory */
		if (add_dir_entry(ROOT_DIR_INUM, inum, name+1) == -1) {
			inode_delete(inum);
			pthread_rwlock_unlock(inode_lock+inum);
			return -1;
		}
		offset = 0;
		pthread_rwlock_unlock(inode_lock+inum);
	}

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}

/* 
 * Loads the content of buffer (no more than len bytes) to the file 
 * given by fhandle 
 */
ssize_t file_write_content(int fhandle, void const *buffer, size_t len){
	pthread_mutex_lock(file_lock+fhandle);
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
		pthread_mutex_unlock(file_lock+fhandle);
        return -1;
    }

	/* Only one write is allowed in the same file at a time */
	pthread_rwlock_wrlock(inode_lock+file->of_inumber);
    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) { 
		pthread_mutex_unlock(file_lock+fhandle);
		pthread_rwlock_unlock(inode_lock+file->of_inumber);
		return -1; 
	}

	/* Buffer's information is to be written one byte at a time */
	char const *cbuffer = (char const*) buffer;

	/* If we are writing to an improper index, return error. This can happen
	 * if O_TRUNC was used on this file since it was oppened to this fhandle */
	if(file->of_offset > inode->i_size){
		pthread_mutex_unlock(file_lock+fhandle);
		pthread_rwlock_unlock(inode_lock+file->of_inumber);
		return -1;
	}

    size_t total_written = 0;
    while (len > 0) {
        size_t starting_block = file->of_offset / BLOCK_SIZE;
		int block_number = get_nth_block(inode, starting_block);

		// If there is no block number starting_block, create one
		if(block_number == -2)
			block_number = (int) inode_alloc_nth_block(inode, starting_block);
		if(block_number == -1){
			pthread_mutex_unlock(file_lock+fhandle);
			pthread_rwlock_unlock(inode_lock+file->of_inumber);
			return -1;
		}

		void* block = data_block_get(block_number);
		if(block == NULL){ 
			pthread_mutex_unlock(file_lock+fhandle);
			pthread_rwlock_unlock(inode_lock+file->of_inumber);
			return -1;
		}

        size_t position = file->of_offset % BLOCK_SIZE;

        size_t write_now = BLOCK_SIZE - position;
		if(write_now > len) write_now = len;

        /* Perform the actual write */
        memcpy(block + position, cbuffer, write_now);

        cbuffer += write_now;
        file->of_offset += write_now;
        total_written += write_now;
        len -= write_now;
    }

	if(file->of_offset > inode->i_size)
		inode->i_size = file->of_offset;

	pthread_mutex_unlock(file_lock+fhandle);
	pthread_rwlock_unlock(inode_lock+file->of_inumber);
	return (ssize_t)total_written;
}

/* 
 * Loads the content of the file given by fhandle (no more than len bytes) 
 * to buffer
 */
ssize_t file_read_content(int fhandle, void *buffer, size_t len){
	/* Only one read is allowed in a open file entry at a time */
	pthread_mutex_lock(file_lock+fhandle);

    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
		pthread_mutex_unlock(file_lock+fhandle);
        return -1;
    }

    /* From the open file table entry, we get the inode */
	/* Several reads are allowed in the same file (different open file entries) */
	pthread_rwlock_rdlock(inode_lock+file->of_inumber);
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
		pthread_rwlock_unlock(inode_lock+file->of_inumber);
		pthread_mutex_unlock(file_lock+fhandle);
        return -1;
    }

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
	if(to_read > len) to_read = len;

	/* Buffer's information is to be read one byte at a time */
	char* cbuffer = (char*) buffer;

	size_t total_read = 0;
	while(to_read > 0){
		size_t starting_block = file->of_offset / BLOCK_SIZE;
		int block_number = get_nth_block(inode, starting_block);
		if(block_number == -1){
			pthread_rwlock_unlock(inode_lock+file->of_inumber);
			pthread_mutex_unlock(file_lock+fhandle);
			return -1;
		}

		void* block = data_block_get(block_number);
		if(block == NULL){
			pthread_rwlock_unlock(inode_lock+file->of_inumber);
			pthread_mutex_unlock(file_lock+fhandle);
			return -1;
		}

		int position = file->of_offset % BLOCK_SIZE;

		size_t read_now = (size_t) (BLOCK_SIZE - position);
		if(read_now > to_read) read_now = to_read;
		
		memcpy(cbuffer, block + position, read_now);
		
		cbuffer += read_now;
		file->of_offset += read_now;
		total_read += read_now;
		to_read -= read_now;
	}

	pthread_rwlock_unlock(inode_lock+file->of_inumber);
	pthread_mutex_unlock(file_lock+fhandle);
	return (ssize_t)total_read;
}

void file_open_lock(){
	pthread_mutex_lock(&fo_lock);
}

void file_open_unlock(){
	pthread_mutex_unlock(&fo_lock);
}
