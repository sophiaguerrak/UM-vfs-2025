// read-write-block.c

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "vfs.h"

/*
    Estas son las funciones de "mas bajo nivel"
    No escriben nada en caso de error, se supone que los invocadores lo controlan
*/

int read_block(const char *image_path, int block_number, void *buffer) {
    int fd = open(image_path, O_RDONLY);
    if (fd < 0)
        return -1;

    if (lseek(fd, block_number * BLOCK_SIZE, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }

    if (read(fd, buffer, BLOCK_SIZE) != BLOCK_SIZE) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int write_block(const char *image_path, int block_number, const void *buffer) {
    int fd = open(image_path, O_WRONLY);
    if (fd < 0)
        return -1;

    if (lseek(fd, block_number * BLOCK_SIZE, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }

    if (write(fd, buffer, BLOCK_SIZE) != BLOCK_SIZE) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int create_block_device(const char *image_path, int total_blocks, int block_size) {
    int fd = open(image_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0)
        return -1;

    char zero[BLOCK_SIZE] = {0};

    for (int i = 0; i < total_blocks; i++) {
        if (write(fd, zero, block_size) != block_size) {
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}
