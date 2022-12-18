#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {

    char *path_copied_file = "/f1";
    char *path_src = "tests/large.txt";
    char buffer_path[8500];
    char buffer_copied[8500];

    assert(tfs_init(NULL) != -1);

    int f;
    size_t bytes_read;
    ssize_t r;

    f = tfs_copy_from_external_fs(path_src, path_copied_file);
    assert(f == -1);

    f = tfs_open(path_copied_file, TFS_O_CREAT);
    assert(f != -1);
    
    FILE* fd = fopen(path_src, "r");

    bytes_read = fread(buffer_path, sizeof(char), sizeof(path_src), fd);

    r = tfs_read(f, buffer_copied, sizeof(buffer_copied) - 1);
    assert(r != bytes_read);
    assert(memcmp(buffer_copied, path_src, strlen(path_src)));

    printf("Successful test.\n");

    return 0;

}