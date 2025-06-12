// superblock.c

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vfs.h"

void print_superblock(const struct superblock *sb) {
    printf("Superblock:\n");
    printf("  Magic: 0x%08X\n", sb->magic);
    printf("  Block size: %u bytes.\n", sb->block_size);
    printf("  Total blocks: %u\n", sb->total_blocks);
    printf("  Superblock blocks: %u\n", sb->superblock_blocks);
    printf("  Inode blocks: %u\n", sb->inode_blocks);
    printf("  Bitmap blocks: %u\n", sb->bitmap_blocks);
    printf("  Free blocks: %u\n", sb->free_blocks);
    printf("  Inode size: %u bytes.\n", sb->inode_size);
    printf("  Inode count: %u\n", sb->inode_count);
    printf("  Free inodes: %u\n", sb->free_inodes);
    printf("  Superblock start block: %u\n", 0);
    printf("  Inode start block: %u\n", sb->inode_start);
    printf("  Bitmap start block: %u\n", sb->bitmap_start);
    printf("  Data start block: %u\n", sb->data_start);
}

int read_superblock(const char *image_path, struct superblock *sb) {
    // Lee el superbloque de la imagen y lo copia en `sb`.
    // Retorna 0 en caso de éxito, -1 en caso de error.
    uint8_t buffer[BLOCK_SIZE];

    if (read_block(image_path, SB_BLOCK_NUMBER, buffer) != 0) {
        fprintf(stderr, "Error al leer el superbloque: %s\n", strerror(errno));
        return -1;
    }

    struct superblock *sb_buf = (struct superblock *)buffer;

    if (sb_buf->magic != MAGIC_NUMBER) {
        fprintf(stderr, "Error: la imagen no contiene un filesystem válido\n");
        return -1;
    }

    memcpy(sb, sb_buf, sizeof(struct superblock));
    return 0;
}

int write_superblock(const char *image_path, struct superblock *sb) {
    // Escribe el superbloque de la imagen a partir de `sb`.
    // Retorna 0 en caso de éxito, -1 en caso de error.

    uint8_t buffer[BLOCK_SIZE] = {0};
    memcpy(buffer, sb, sizeof(struct superblock));

    if (sb->magic != MAGIC_NUMBER) {
        fprintf(stderr, "Error: la estructura no contiene un MAGIC_NUMBER válido\n");
        return -1;
    }

    if (write_block(image_path, SB_BLOCK_NUMBER, buffer) != 0) {
        fprintf(stderr, "Error al escribir el superbloque: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int init_superblock(const char *image_path, uint32_t total_blocks, uint32_t total_inodes) {

    uint8_t superblock_buffer[BLOCK_SIZE] = {0};
    // Acceder a la estructura de superbloque usando un puntero
    struct superblock *sb = (struct superblock *)superblock_buffer;

    sb->magic = MAGIC_NUMBER;
    sb->block_size = BLOCK_SIZE;
    sb->total_blocks = sb->free_blocks = total_blocks;
    sb->superblock_blocks = 1;
    sb->inode_blocks = total_inodes / INODES_PER_BLOCK;
    sb->bitmap_blocks = (sb->total_blocks + BITS_PER_BLOCK - 1) / BITS_PER_BLOCK;
    sb->inode_count = total_inodes;
    sb->inode_size = INODE_SIZE;
    sb->free_inodes = total_inodes;
    sb->inode_start = sb->superblock_blocks;
    sb->bitmap_start = sb->inode_start + sb->inode_blocks;
    sb->data_start = sb->bitmap_start + sb->bitmap_blocks;

    // Inicializar bitmap_zeroes[]
    sb->bitmap_zeroes[0] = BITS_PER_BLOCK - sb->data_start;
    for (uint32_t i = 1; i < sb->bitmap_blocks; i++) {
        sb->bitmap_zeroes[i] = BITS_PER_BLOCK;
    }

    if (write_block(image_path, SB_BLOCK_NUMBER, superblock_buffer) != 0) {
        fprintf(stderr, "Error: no se pudo escribir el superbloque\n");
        return -1;
    }

    sb->free_blocks = sb->total_blocks; // cada invocación a bitmap_set_first_free lo decrementa
    for (uint32_t i = 0; i < sb->data_start; i++) {
        // prende el bit en el bitmap y decrementa free_blocks

        int first_free = bitmap_set_first_free(image_path);
        DEBUG_PRINT("first free block %u.\n", first_free);

        if (first_free != (int)i) {
            fprintf(stderr, "Error inesperado asignando bloques libres\n");
            return -1;
        }
    }

    return 0;
}
