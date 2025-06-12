// vfs-copy.c

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vfs.h"

// Copia un archivo del sistema anfitrión al filesystem virtual.
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s imagen archivo_origen nombre_destino\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *image_path = argv[1];
    const char *host_file = argv[2];
    const char *dest_name = argv[3];

    // Verificar imagen
    struct superblock sb_struct, *sb = &sb_struct;
    
    if (read_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al leer superblock\n");
        return EXIT_FAILURE;
    }

    // Verificar nombre válido
    if (!name_is_valid(dest_name)) {
        fprintf(stderr, "Nombre inválido: %s\n", dest_name);
        return EXIT_FAILURE;
    }

    // Verificar si ya existe en el directorio
    if (dir_lookup(image_path, dest_name) != 0) {
        fprintf(stderr, "El nombre '%s' ya existe en el directorio\n", dest_name);
        return EXIT_FAILURE;
    }

    // Abrir archivo origen
    int fd = open(host_file, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error (%s) al abrir archivo %s\n", strerror(errno), host_file);
        return EXIT_FAILURE;
    }

    // Obtener tamaño
    struct stat st;
    if (fstat(fd, &st) != 0) {
        fprintf(stderr, "Error al obtener tamaño de archivo %s.\n", host_file);
        close(fd);
        return EXIT_FAILURE;
    }

    uint16_t perms = st.st_mode & 0777;
    
    // Crear nodo-I vacío
    int new_inode = create_empty_file_in_free_inode(image_path, perms);
    if (new_inode < 0) {
        fprintf(stderr, "Error al crear archivo destino en VFS\n");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // Agregar entrada al directorio raíz
    if (add_dir_entry(image_path, dest_name, new_inode) != 0) {
        fprintf(stderr, "Error al agregar entrada de directorio para %s\n", dest_name);
        return EXIT_FAILURE;
    }
    
    // Leer y escribir por bloques
    uint8_t buffer[BLOCK_SIZE];
    ssize_t nread;

    for (size_t offset = 0; (nread = read(fd, buffer, BLOCK_SIZE)) != 0; offset += nread) {

        if (nread < 0) {
            fprintf(stderr, "Error al leer archivo origen %s\n", host_file);
            close(fd);
            return EXIT_FAILURE;
        }

        if (inode_write_data(image_path, new_inode, buffer, nread, offset) != nread) {
            fprintf(stderr, "Error al escribir datos en VFS, nodo-I nro %d, nread %zu, offset %zd.\n", new_inode, nread, offset);
            close(fd);
            return EXIT_FAILURE;
        }
    }

    close(fd);

    DEBUG_PRINT("Archivo copiado exitosamente como '%s' (inode %d)\n", dest_name, new_inode);
    return EXIT_SUCCESS;
}
