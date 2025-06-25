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

    // Validar y cargar superbloque
    struct superblock sb;
    if (read_superblock(image_path, &sb) != 0) {
        perror("Error al leer el superbloque");
        return EXIT_FAILURE;
    }

    // Para cada archivo a crear
    for (int i = 2; i < argc; i++) {
        const char *filename = argv[i];

        // Validar nombre
        if (!name_is_valid(filename)) {
            fprintf(stderr, "Nombre inválido: %s\n", filename);
            continue;
        }

        // Verificar si ya existe
        int existing_inode = dir_lookup(image_path, filename);
        if (existing_inode > 0) {
            fprintf(stderr, "Ya existe un archivo con el nombre: %s\n", filename);
            continue;
        }

        // Crear inodo vacío para archivo regular
        int new_inode = create_empty_file_in_free_inode(image_path, INODE_MODE_FILE | 0644);
        if (new_inode < 0) {
            fprintf(stderr, "Error al crear inodo para: %s\n", filename);
            continue;
        }

        // Agregar entrada al directorio raíz
        if (add_dir_entry(image_path, filename, new_inode) != 0) {
            fprintf(stderr, "Error al agregar %s al directorio\n", filename);
            // Limpiar el inodo si falló
            free_inode(image_path, new_inode);
            continue;
        }

        printf("Archivo '%s' creado exitosamente (inodo %d)\n", filename, new_inode);
    }

    return EXIT_SUCCESS;
}

