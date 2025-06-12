// vfs-mkfs.c

#include "vfs.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t round_up_inodes(uint32_t count) {
    // funcion local que redondea la cantidad de inodos para ocupar los bloques por completo
    return ((count + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK * INODES_PER_BLOCK);
}

/*
    Crea el filesystem "vacio":
        Bloque 0: superblock
        Bloques 1 a N: area de nodos-I, el nodo-I 0 no se usa, el 1 es el directorio raiz
        Bloques N+1 a B: area de bitmap de bloques ocupados/libres
        Bloque B+1: directorio raiz (unico), solo con entradas . y ..
*/
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <nombre_imagen> <total_bloques> <cantidad_nodosI>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *image_path = argv[1];

    uint32_t total_blocks = (uint32_t)atoi(argv[2]);
    if (total_blocks < VFS_MIN_BLOCKS || total_blocks >= VFS_MAX_BLOCKS) {
        fprintf(stderr, "Error: total_bloques debe ser un entero entre %d y %d.\n", VFS_MIN_BLOCKS, VFS_MAX_BLOCKS);
        return EXIT_FAILURE;
    }

    uint32_t cantidad_nodosI = (uint32_t)atoi(argv[3]);
    if (cantidad_nodosI < INODES_PER_BLOCK || cantidad_nodosI >= total_blocks) {
        fprintf(stderr, "Error: cantidad_nodosI debe ser mayor a %zu y no mayor a la cantidad de bloques.\n",
                INODES_PER_BLOCK);
        return EXIT_FAILURE;
    }

    errno = 0;
    int result = create_block_device(image_path, total_blocks, BLOCK_SIZE);
    if (result != 0) {
        fprintf(stderr, "Error al crear el dispositivo de bloques: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Dispositivo de bloques creado exitosamente: %s\n", image_path);

    uint32_t total_inodes = round_up_inodes(cantidad_nodosI);

    if (init_superblock(image_path, total_blocks, total_inodes) != 0) {
        fprintf(stderr, "Error: no se pudo inicializar el superbloque\n");
        return EXIT_FAILURE;
    }

    if (create_root_dir(image_path) != 0) {
        fprintf(stderr, "Error: no se pudo crear el directorio raíz\n");
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Dispositivo de bloques inicializado exitosamente: %s\n", image_path);

    return EXIT_SUCCESS;
}
