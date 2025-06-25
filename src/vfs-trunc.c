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

        // Buscar inodo del archivo
        int inode_number = dir_lookup(image_path, filename);
        if (inode_number <= 0) {
            fprintf(stderr, "Archivo '%s' no encontrado.\n", filename);
            continue;
        }

        struct inode in;
        if (read_inode(image_path, inode_number, &in) != 0) {
            fprintf(stderr, "No se pudo leer el inodo de '%s'\n", filename);
            continue;
        }

        // Verificar que sea un archivo regular
        if ((in.mode & INODE_MODE_FILE) != INODE_MODE_FILE) {
            fprintf(stderr, "'%s' no es un archivo regular.\n", filename);
            continue;
        }

        // Liberar bloques de datos
        for (uint16_t j = 0; j < in.blocks; j++) {
            int block_nbr = get_block_number_at(image_path, &in, j);
            if (block_nbr > 0) {
                bitmap_free_block(image_path, block_nbr);
            }
        }

        // Truncar archivo
        in.size = 0;
        in.blocks = 0;
        if (write_inode(image_path, inode_number, &in) != 0) {
            fprintf(stderr, "No se pudo escribir el inodo truncado de '%s'\n", filename);
            continue;
        }

        printf("Archivo '%s' truncado exitosamente.\n", filename);
    }

    return 0;
}

