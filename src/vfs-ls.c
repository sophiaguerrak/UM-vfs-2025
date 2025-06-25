#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "vfs.h"

// Este programa lista los archivos del directorio raíz al estilo ls -l
int main(int argc, char *argv[]) {
    // Verifica que se pase la imagen como argumento
    if (argc != 2) {
        fprintf(stderr, "Uso: %s imagen\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *image_path = argv[1];

    // Lee el superbloque para saber la cantidad de inodos
    struct superblock sb;
    if (read_superblock(image_path, &sb) != 0) {
        fprintf(stderr, "Error al leer el superbloque\n");
        return EXIT_FAILURE;
    }

    // Lee el inodo del directorio raíz (siempre es el inodo 1)
    struct inode root_inode;
    if (read_inode(image_path, 1, &root_inode) != 0) {
        fprintf(stderr, "Error al leer el inodo del directorio raíz\n");
        return EXIT_FAILURE;
    }

    // Lee el bloque de datos del directorio raíz
    uint8_t buffer[BLOCK_SIZE];
    int data_block = get_block_number_at(image_path, &root_inode, 0);
    if (data_block < 0) {
        fprintf(stderr, "Error al obtener el bloque de datos del directorio raíz\n");
        return EXIT_FAILURE;
    }
    if (read_block(image_path, data_block, buffer) != 0) {
        fprintf(stderr, "Error al leer el bloque de datos del directorio raíz\n");
        return EXIT_FAILURE;
    }

    // Recorre todas las entradas de directorio y muestra la información estilo ls -l
    struct dir_entry *entry = (struct dir_entry *)buffer;
    int entries = BLOCK_SIZE / sizeof(struct dir_entry);

    for (int i = 0; i < entries; i++) {
        if (entry[i].inode == 0) continue; // Entrada vacía

        // Lee el inodo del archivo
        struct inode in;
        if (read_inode(image_path, entry[i].inode, &in) != 0) {
            fprintf(stderr, "Error al leer inodo %d\n", entry[i].inode);
            continue;
        }

        // Muestra los datos estilo ls -l
        print_inode(&in, entry[i].inode, entry[i].name);
    }

    return EXIT_SUCCESS;
}
