#include "vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s imagen archivo1 [archivo2...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *image_path = argv[1];

    for (int i = 2; i < argc; i++) {
        const char *filename = argv[i];

        if (!name_is_valid(filename)) {
            fprintf(stderr, "Nombre inválido: %s\n", filename);
            continue;
        }

        int inode_nbr = dir_lookup(image_path, filename);
        if (inode_nbr <= 0) {
            fprintf(stderr, "Archivo no encontrado: %s\n", filename);
            continue;
        }

        // Eliminar entrada del directorio
        if (remove_dir_entry(image_path, filename) != 0) {
            fprintf(stderr, "Error al eliminar entrada de directorio: %s\n", filename);
            continue;
        }

        // Liberar inodo asociado
        if (free_inode(image_path, inode_nbr) != 0) {
            fprintf(stderr, "Error al liberar inodo %d de archivo %s\n", inode_nbr, filename);
            continue;
        }

        printf("Archivo '%s' eliminado correctamente (inodo %d)\n", filename, inode_nbr);
    }

    return EXIT_SUCCESS;
}

