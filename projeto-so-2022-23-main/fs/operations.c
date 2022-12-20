#include "operations.h"
#include "config.h"
#include "state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "betterassert.h"

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}

int tfs_init(tfs_params const *params_ptr) {
    tfs_params params;
    if (params_ptr != NULL) {
        params = *params_ptr;
    } else {
        params = tfs_default_params();
    }

    if (state_init(params) != 0) {
        return -1;
    }

    // create root inode
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name
 *   - root_inode: the root directory inode
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(char const *name, inode_t const *root_inode) {
    if (!valid_pathname(name)) {
        return -1;
    }

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_lookup: root dir inode must exist");
    if (root_dir_inode != root_inode)
        return -1;

    // skip the initial '/' character
    name++;
    if (inode_read_lock(root_dir_inode) != 0)
        return -1;

    int inumber = find_in_dir(root_inode, name);

    if (inumber == -1) {
        inode_unlock(root_dir_inode);
        return -1;
    }
    if (inode_unlock(root_dir_inode) != 0)
        return -1;

    inode_t *inode = inode_get(inumber);
    if (inode == NULL) {
        return -1;
    }

    // if the target inode is of the SYM link type, keep searching for the
    // associated file recursively
    if (inode->i_node_type == T_SYMB_LINK) {
        char *path = (char *)data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(path != NULL, "tfs_lookup: data block deleted mid-write");

        inumber = tfs_lookup(path, root_inode);
        if (inumber == -1)
            return -1;

        inode = inode_get(inumber);
        if (inode == NULL)
            return -1;
    }

    return inumber;
}

int tfs_open(char const *name, tfs_file_mode_t mode) {
    // Checks if the path name is valid
    if (!valid_pathname(name)) {
        return -1;
    }

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");

    // only one file can be open at a time
    int inum = tfs_lookup(name, root_dir_inode);
    size_t offset;

    if (inum >= 0) {
        if (inum_write_lock(inum) != 0)
            return -1;
        // The file already exists
        inode_t *inode = inode_get(inum);
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");

        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                if (data_block_free(inode->i_data_block) != 0)
                    return -1;
                inode->i_size = 0;
            }
        }
        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
        if (inum_unlock(inum) != 0)
            return -1;

    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1; // no space in inode table
        }

        // Add entry in the root directory
        if (inum_write_lock(inum) != 0)
            return -1;

        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inum_unlock(inum);
            inode_delete(inum);
            return -1; // no space in directory
        }

        offset = 0;
        if (inum_unlock(inum) != 0)
            return -1;
    } else {
        return -1;
    }
    // Finally, add entry to the open file table and return the corresponding
    // handle
    return add_to_open_file_table(inum, offset);
    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

/**
 * @brief Creates a sym link to a file.
 *
 * @param target target file's path
 * @param link_name name to be given to the new link
 * @return int 0 if successful, -1 if not
 */
int tfs_sym_link(char const *target, char const *link_name) {
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_sym_link: root dir inode must exist");

    // target's path cant be bigger than block size
    if (sizeof(target) / sizeof(char *) > state_block_size())
        return -1;

    int target_inum = tfs_lookup(target, root_dir_inode);
    if (target_inum == -1)
        return -1;

    inode_t *target_inode = inode_get(target_inum);
    if (target_inode == NULL)
        return -1;

    // create new inode for the sym link
    int link_inum = inode_create(T_SYMB_LINK);
    if (link_inum == -1)
        return -1;

    inode_t *link = inode_get(link_inum);
    if (link == NULL) {
        inode_delete(link_inum);
        return -1;
    }

    // alloc new data block for the sym link inode
    int bnum = data_block_alloc();
    if (bnum == -1) {
        inode_delete(link_inum);
        return -1;
    }

    // set the allocated data block as the inode's data block
    link->i_data_block = bnum;

    void *block = data_block_get(link->i_data_block);
    ALWAYS_ASSERT(block != NULL, "tfs_sym_link: data block deleted mid-write");

    // copy target's path to sym link's data block
    memcpy(block, target, strlen(target));

    // set sym link's data block size
    link->i_size = sizeof(target) / sizeof(char const *);

    if (add_dir_entry(root_dir_inode, link_name + 1, link_inum) == -1) {
        inode_delete(link_inum);
        return -1;
    }

    return 0;
}

/**
 * @brief Create a hard link to a file.
 *
 * @param target target file's path
 * @param link_name name to be given to the new link
 * @return int 0 if successful, -1 if not
 */
int tfs_link(char const *target, char const *link_name) {
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_link: root dir inode must exist");

    if (inode_read_lock(root_dir_inode) != 0)
        return -1;

    int target_inumber = find_in_dir(root_dir_inode, target + 1);

    if (target_inumber == -1) {
        inode_unlock(root_dir_inode);
        return -1;
    }
    if (inode_unlock(root_dir_inode) != 0)
        return -1;

    inode_t *target_inode = inode_get(target_inumber);
    // if the target's inode is a sym link, return -1
    if (target_inode == NULL || target_inode->i_node_type == T_SYMB_LINK)
        return -1;

    if (add_dir_entry(root_dir_inode, link_name + 1, target_inumber) == -1) {
        return -1;
    }

    // increment the hard link counter of the target's inode
    target_inode->hard_link_counter++;

    return 0;
}

int tfs_close(int fhandle) {
    if (open_file_lock(fhandle) != 0)
        return -1;

    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        open_file_unlock(fhandle);
        return -1; // invalid fd
    }

    if (remove_from_open_file_table(fhandle) != 0) {
        open_file_unlock(fhandle);
        return -1;
    }

    if (open_file_unlock(fhandle) != 0)
        return -1;

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    if (open_file_lock(fhandle) != 0)
        return -1;

    open_file_entry_t *file = get_open_file_entry(fhandle);

    if (file == NULL) {
        open_file_unlock(fhandle);
        return -1;
    }

    //  From the open file table entry, we get the inode
    if (inum_write_lock(file->of_inumber) != 0)
        return -1;

    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");

    // Determine how many bytes to write
    size_t block_size = state_block_size();
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }

    if (to_write > 0) {
        if (inode->i_size == 0) {
            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                inum_unlock(file->of_inumber);
                open_file_unlock(fhandle);
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");

        // Perform the actual write
        memcpy((char *)block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }

    if (inum_unlock(file->of_inumber) != 0)
        return -1;

    if (open_file_unlock(fhandle) != 0)
        return -1;

    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    if (open_file_lock(fhandle) != 0)
        return -1;

    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        open_file_unlock(fhandle);
        return -1;
    }

    // From the open file table entry, we get the inode
    if (inum_write_lock(file->of_inumber) != 0)
        return -1;

    inode_t const *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_read: data block deleted mid-read");

        // Perform the actual read
        memcpy(buffer, (char *)block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }

    if (inum_unlock(file->of_inumber) != 0)
        return -1;

    if (open_file_unlock(fhandle) != 0)
        return -1;
        
    return (ssize_t)to_read;
}

/**
 * @brief
 *
 * @param target
 * @return int
 */
int tfs_unlink(char const *target) {
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_unlink: root dir inode must exist");

    if (inode_read_lock(root_dir_inode) != 0)
        return -1;
    int target_inum = find_in_dir(root_dir_inode, target + 1);
    if (target_inum == -1) {
        inode_unlock(root_dir_inode);
        return -1;
    }
    if (inode_unlock(root_dir_inode) != 0)
        return -1;
    inode_t *target_inode = inode_get(target_inum);
    if (target_inode == NULL || target_inode->i_node_type == T_DIRECTORY)
        return -1;

    // if the target file is open, return -1
    int is_open = is_open_file(target_inum);
    if (is_open != 0)
        return -1;

    if (clear_dir_entry(root_dir_inode, target + 1) == -1)
        return -1;

    target_inode->hard_link_counter--;

    if (target_inode->hard_link_counter == 0)
        if (inode_delete(target_inum) != 0)
            return -1;

    return 0;
}

/**
 * @brief
 *
 * @param source_path
 * @param dest_path
 * @return int
 */
int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {
    FILE *f_read = fopen(source_path, "r");
    if (f_read == NULL)
        return -1;

    int f_write = tfs_open(dest_path, TFS_O_CREAT);
    if (f_write == -1) {
        fclose(f_read);
        return -1;
    }

    char *buffer[state_block_size()];
    memset(buffer, 0, sizeof(buffer) / sizeof(char *));

    size_t bytes_read = 0;

    bytes_read =
        fread(buffer, sizeof(char), sizeof(buffer) / sizeof(char *), f_read);
    if (ferror(f_read)) {
        fclose(f_read);
        tfs_close(f_write);
        return -1;
    }

    ssize_t bytes_written = tfs_write(f_write, buffer, bytes_read);
    if (bytes_written == -1) {
        fclose(f_read);
        tfs_close(f_write);
        return -1;
    }

    // if f_read's size greter than data block's size, return -1
    if (!feof(f_read)) {
        fclose(f_read);
        tfs_close(f_write);
        return -1;
    }

    if (tfs_close(f_write) == -1) {
        fclose(f_read);
        return -1;
    }

    if (fclose(f_read) == EOF)
        return -1;

    return 0;
}
