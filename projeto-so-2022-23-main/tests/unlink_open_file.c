#include "../fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char *file_path = "/f1";
    const char *link_path = "/l1";

    assert(tfs_init(NULL) != -1);

    // Create file
    int fd = tfs_open(file_path, TFS_O_CREAT);
    assert(fd != -1);

    assert(tfs_close(fd) != -1);

    assert(tfs_link(file_path, link_path) != -1);

    fd = tfs_open(link_path, TFS_O_CREAT);
    assert(fd != -1);

    // Try to unlink open file
    assert(tfs_unlink(file_path) == -1);
    assert(tfs_unlink(link_path) == -1);

    // Close the file
    fd = tfs_close(fd);
    assert(fd != -1);

    // Unlink now closed file
    assert(tfs_unlink(link_path) != -1);
    assert(tfs_unlink(file_path) != -1);

    printf("Successful test.\n");

    return 0;
}
