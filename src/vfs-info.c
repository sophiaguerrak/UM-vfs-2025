// vfs-info.c

#include "vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s imagen\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *image_path = argv[1];
    struct superblock sb_struct;
    
    if (read_superblock(image_path, &sb_struct) != 0) {
        fprintf(stderr, "Error al leer superblock\n");
        return EXIT_FAILURE;
    }

    print_superblock(&sb_struct);
    
    uint8_t buffer[BLOCK_SIZE];
    printf("\nBlock bitmap:\n");
    uint32_t to_print = sb_struct.total_blocks;
    for (uint32_t i = 0; i < sb_struct.bitmap_blocks; i++)
    {
        if (read_block(image_path, sb_struct.bitmap_start + i, buffer) != 0)
        {
            fprintf(stderr, "Error al leer bloque de bitmap %u\n", i);
            return EXIT_FAILURE;
        }

        print_bitmap_block(buffer, to_print < BLOCK_SIZE ? to_print : BLOCK_SIZE);
        to_print -= BLOCK_SIZE;
    }

    return EXIT_SUCCESS;
}