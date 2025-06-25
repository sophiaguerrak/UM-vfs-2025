#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vfs.h"

struct file_entry {
    char name[FILENAME_MAX_LEN];
    struct inode in;
    uint32_t inode_nbr;
};

int compare_entries(const void *a, const void *b) {
    const struct file_entry *fa = a;
    const struct file_entry *fb = b;
    return strcmp(fa->name, fb->name);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <imagen>\n", argv[0]);
        return 1;
    }

    const char *image_path = argv[1];
    struct inode root_inode;

    if (read_inode(image_path, ROOTDIR_INODE, &root_inode) != 0) {
        fprintf(stderr, "No se pudo leer el directorio raíz\n");
        return 1;
    }

    struct file_entry files[DIR_ENTRIES_PER_BLOCK * root_inode.blocks];
    int file_count = 0;

    for (int i = 0; i < root_inode.blocks; i++) {
        int block_num = get_block_number_at(image_path, &root_inode, i);
        if (block_num <= 0) continue;

        uint8_t data_buf[BLOCK_SIZE];
        if (read_block(image_path, block_num, data_buf) != 0) continue;

        struct dir_entry *entries = (struct dir_entry *)data_buf;
        for (int j = 0; j < DIR_ENTRIES_PER_BLOCK; j++) {
            if (entries[j].inode == 0) continue;

            if (read_inode(image_path, entries[j].inode, &files[file_count].in) == 0) {
                strncpy(files[file_count].name, entries[j].name, FILENAME_MAX_LEN);
                files[file_count].inode_nbr = entries[j].inode;
                file_count++;
            }
        }
    }

    qsort(files, file_count, sizeof(struct file_entry), compare_entries);

    for (int i = 0; i < file_count; i++) {
        print_inode(&files[i].in, files[i].inode_nbr, files[i].name);
    }

    return 0;
}

