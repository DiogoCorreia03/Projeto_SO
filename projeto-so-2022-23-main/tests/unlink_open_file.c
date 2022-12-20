#include "../fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {

    // test to check if its possible to unlink an open file

    const char *file_path = "/f1";
    const char *link_path = "/l1";
    const char *link_path2 = "/l2";

    assert(tfs_init(NULL) != -1);

    // Create file
    int fd = tfs_open(file_path, TFS_O_CREAT);
    assert(fd != -1);

    assert(tfs_close(fd) != -1);

    // Create a link to the file
    assert(tfs_link(file_path, link_path) != -1);
    assert(tfs_sym_link(file_path, link_path2) != -1);

    // Open the file
    fd = tfs_open(file_path, TFS_O_CREAT);
    assert(fd != -1);

    // Try to unlink open file
    assert(tfs_unlink(file_path) == -1);
    assert(tfs_unlink(link_path) == -1);
    assert(tfs_unlink(link_path2) == -1);

    // Close the file
    fd = tfs_close(fd);
    assert(fd != -1);

    // Unlink closed file
    assert(tfs_unlink(link_path) != -1);
    assert(tfs_unlink(link_path2) != -1);
    assert(tfs_unlink(file_path) != -1);


    printf("Successful test.\n");

    return 0;
}
