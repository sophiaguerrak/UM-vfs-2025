// src/vfs-cat.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "vfs.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <imagen> <archivo1> [archivo2...]\n", argv[0]);
        return 1;
    }

    const char *image_path = argv[1];

    for (int i = 2; i < argc; i++) {
        const char *filename = argv[i];

        int inode_number = dir_lookup(image_path, filename);
        if (inode_number <= 0) {
            fprintf(stderr, "Archivo '%s' no encontrado en la imagen\n", filename);
            continue;
        }

        struct inode in;
        if (read_inode(image_path, inode_number, &in) != 0) {
            fprintf(stderr, "Error al leer el inodo del archivo '%s'\n", filename);
            continue;
        }

        if ((in.mode & INODE_MODE_FILE) != INODE_MODE_FILE) {
            fprintf(stderr, "'%s' no es un archivo regular\n", filename);
            continue;
        }

        uint32_t bytes_remaining = in.size;
        for (uint16_t j = 0; j < in.blocks && bytes_remaining > 0; j++) {
            int block_num = get_block_number_at(image_path, &in, j);
            if (block_num <= 0) {
                fprintf(stderr, "Error al obtener bloque %d del archivo '%s'\n", j, filename);
                break;
            }

            uint8_t buffer[BLOCK_SIZE];
            if (read_block(image_path, block_num, buffer) != 0) {
                fprintf(stderr, "Error al leer bloque %d del archivo '%s'\n", block_num, filename);
                break;
            }

            size_t to_print = (bytes_remaining < BLOCK_SIZE) ? bytes_remaining : BLOCK_SIZE;
            fwrite(buffer, 1, to_print, stdout);
            bytes_remaining -= to_print;
        }
    }

    return 0;
}

