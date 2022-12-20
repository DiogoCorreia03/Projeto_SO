#include "../fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint8_t const file_contents[] = "SOU LINDO";
char const target_path1[] = "/f1";
char const link_path1[] = "/l1";
char const target_path2[] = "/f2";
char const link_path2[] = "/l2";
char const target_path3[] = "/f3";
char const link_path3[] = "/l3";
char const link_path4[] = "/l4";
char const link_path5[] = "/l5";

void assert_empty_file(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void assert_contents_ok(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void write_contents(char const *path) {
    int f = tfs_open(path, 0);
    assert(f != -1);

    assert(tfs_write(f, file_contents, sizeof(file_contents)) ==
           sizeof(file_contents));

    assert(tfs_close(f) != -1);
}

int main() {

    // overrall test for sym / hard links

    assert(tfs_init(NULL) != -1);

    // ---------------criação e unlink de um hard link-----------------------------------

    int f = tfs_open(target_path1, TFS_O_CREAT);
    assert(f != -1);
    assert(tfs_close(f) != -1);
    assert_empty_file(target_path1);
    
    assert(tfs_link(target_path1, link_path1) == 0);
    assert_empty_file(link_path1);

    write_contents(target_path1);
    assert_contents_ok(link_path1);

    assert(tfs_unlink(link_path1) != -1);

    assert(tfs_open(link_path1, 0) == -1);

    // ---------------criação e unlink de um sym link------------------------------------

    int d = tfs_open(target_path2, TFS_O_CREAT);
    assert(d != -1);
    assert(tfs_close(d) != -1);
    assert_empty_file(target_path2);

    assert(tfs_sym_link(target_path2, link_path2) == 0);
    assert_empty_file(link_path2);

    write_contents(target_path2);
    assert_contents_ok(link_path2);

    assert(tfs_unlink(link_path2) != -1);

    assert(tfs_open(link_path2, 0) == -1);

    // --------------criação de vãrios sym link e leitura do ficheiro---------------------
    
    int g = tfs_open(target_path3, TFS_O_CREAT);
    assert(g != -1);
    assert(tfs_close(g) != -1);
    assert_empty_file(target_path3);

    assert(tfs_sym_link(target_path3, link_path3) != -1);
    assert_empty_file(link_path3);

    assert(tfs_sym_link(link_path3, link_path4) != -1);
    assert_empty_file(link_path4);

    assert(tfs_sym_link(link_path4, link_path5) != -1);
    assert_empty_file(link_path5);

    write_contents(link_path5);
    assert_contents_ok(target_path3);
    assert_contents_ok(link_path3);
    assert_contents_ok(link_path4);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;
}