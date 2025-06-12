// rootdir.c

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "vfs.h"

int create_root_dir(const char *image_path) {

    struct superblock sb_str, *sb = &sb_str;

    if (read_superblock(image_path, sb) != 0) {
        return -1;
    }

    if (sb->free_inodes == 0 || sb->free_blocks == 0) {
        return -1;
    }

    // Reservar un bloque de datos para el directorio
    int rootdir_data_block = bitmap_set_first_free(image_path);
    DEBUG_PRINT("rootdir block %d.\n", rootdir_data_block);
    if (rootdir_data_block == -1) {
        fprintf(stderr, "No hay bloques disponibles para el bloque del directorio raiz.\n");
        return -1;
    }

    // Agregar las entradas . y .. del directorio raiz, que son referencias a si mismo
    uint8_t data_buffer[BLOCK_SIZE] = {0};
    struct dir_entry *entries = (struct dir_entry *)data_buffer;
    entries[0].inode = ROOTDIR_INODE;
    strncpy(entries[0].name, ".", FILENAME_MAX_LEN);
    entries[1].inode = ROOTDIR_INODE;
    strncpy(entries[1].name, "..", FILENAME_MAX_LEN);

    // Actualizar y escribir el bloque de datos del directorio
    if (write_block(image_path, rootdir_data_block, data_buffer) != 0) {
        return -1;
    }

    // Crear el nodo-i en la posicion ROOTDIR_INODE (directorio raíz)
    struct inode in = {0};  // por defecto, todo inicializado en 0
    in.mode = INODE_MODE_DIR | 0755;
    in.uid = getuid();
    in.gid = getgid();
    in.blocks = 1;
    in.size = BLOCK_SIZE;
    in.direct[0] = rootdir_data_block;
    time_t now = time(NULL);
    in.atime = in.mtime = in.ctime = (uint32_t)now;

    if (write_inode(image_path, ROOTDIR_INODE, &in) != 0)
        return -1;

    // Actualizar y re-escribir el superbloque
    if (read_superblock(image_path, sb) != 0) {
        return -1;
    }
    sb->free_inodes--;
    if (write_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error: no se pudo escribir el superbloque\n");
        return -1;
    }

    return 0;
}