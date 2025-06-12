// read-write-data.c

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "vfs.h"

int inode_write_data(const char *image_path, uint32_t inode_number, void *data_buf, size_t len, size_t offset) {
    // Escribe datos en un archivo, desde un offset dado.
    // Asegura que se asignen bloques si es necesario.
    // Debe retornar len si todo anda bien

    DEBUG_PRINT("inode_write_data inode_number %d, len %zu, offset %zu.\n", inode_number, len, offset);

    struct inode in;
    if (read_inode(image_path, inode_number, &in) != 0) {
        fprintf(stderr, "Error al leer el inodo %d\n", inode_number);
        return -1;
    }

    // Verificar que offset no supere el tamaño máximo posible
    size_t max_file_size = (NUM_DIRECT_PTRS + NUM_INDIRECT_PTRS) * BLOCK_SIZE;
    if (offset + len > max_file_size) {
        fprintf(stderr, "Error: Escritura supera el tamaño máximo permitido del archivo\n");
        return -1;
    }

    // Leer el superbloque para validar si hay bloques disponibles
    struct superblock sb_struct, *sb = &sb_struct;

    if (read_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al leer superblock\n");
        return -1;
    }

    // Calcular cuántos bloques necesita el archivo para abarcar hasta offset + len
    size_t final_size = offset + len;
    size_t required_blocks = (final_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    DEBUG_PRINT("final_size %zu, required_blocks %zu in.blocks %u.\n", final_size, required_blocks, in.blocks);

    if (required_blocks > in.blocks) {
        size_t to_allocate = required_blocks - in.blocks;
        DEBUG_PRINT("final_size %zu, required_blocks %zu to_allocate %zu.\n", final_size, required_blocks, to_allocate);

        // Validar si hay suficientes bloques libres
        if (to_allocate > sb->free_blocks) {
            fprintf(stderr, "Error: No hay bloques libres suficientes (%zu requeridos)\n", to_allocate);
            return -1;
        }

        // Asignar los bloques necesarios
        for (size_t i = 0; i < to_allocate; i++) {
            int new_block = bitmap_set_first_free(image_path);
            DEBUG_PRINT("bloque adicional es %d\n", new_block);

            if (new_block == -1) {
                fprintf(stderr, "Error al asignar bloque adicional\n");
                return -1;
            }

            if (inode_append_block(image_path, &in, new_block) != 0) {
                return -1;
            }
        }
    }

    // Empezar a escribir los datos
    uint8_t block_buf[BLOCK_SIZE];
    uint8_t *src = (uint8_t *)data_buf;
    size_t remaining = len;

    size_t start_block = offset / BLOCK_SIZE;
    size_t start_offset = offset % BLOCK_SIZE;

    DEBUG_PRINT("start_block: %zd start_offset: %zd.\n", start_block, start_offset);

    for (size_t i = start_block; remaining > 0; i++) {
        int block_num = get_block_number_at(image_path, &in, i);
        if (block_num <= 0) {
            fprintf(stderr, "Error inesperado obteniendo el bloque número %zu del archivo\n", i);
            return -1;
        }

        // Leer el bloque actual del archivo
        if (read_block(image_path, block_num, block_buf) != 0) {
            fprintf(stderr, "Error inesperado leyendo bloque %d\n", block_num);
            return -1;
        }

        // Calcular cuánto escribir en este bloque
        size_t write_offset = (i == start_block) ? start_offset : 0;
        size_t space = BLOCK_SIZE - write_offset;
        size_t to_write = (remaining < space) ? remaining : space;

        memcpy(block_buf + write_offset, src, to_write);

        DEBUG_PRINT("Escribiendo bloque %d, Write offset %zu, space %zu, towrite %zu.\n", block_num, write_offset,
                    space, to_write);

        if (write_block(image_path, block_num, block_buf) != 0) {
            fprintf(stderr, "Error escribiendo bloque %d\n", block_num);
            return -1;
        }

        src += to_write;
        remaining -= to_write;
    }

    // Actualizar tamaño si se escribió más allá del tamaño anterior
    if (offset + len > in.size) {
        in.size = offset + len;
    }

    // Actualizar tiempos
    DEBUG_PRINT("inode_write_data: atime valor anterior %u.\n", in.atime);
    DEBUG_PRINT("inode_write_data: mtime valor anterior %u.\n", in.mtime);
    time_t now = time(NULL);
    in.mtime = in.atime = (uint32_t)now;
    DEBUG_PRINT("Actualizando atime y mtime del inodo %u a %u.\n", inode_number, in.atime);

    // Escribir el inodo actualizado al final
    if (write_inode(image_path, inode_number, &in) != 0) {
        fprintf(stderr, "Error al escribir el inodo %d\n", inode_number);
        return -1;
    }

    return len;
}

int inode_read_data(const char *image_path, uint32_t inode_number, void *data_buf, size_t len, size_t offset) {
    // Lee datos desde un archivo, a partir de un offset dado, hasta len bytes.
    // Retorna 0 si todo fue bien, -1 si hubo error.
    DEBUG_PRINT("inode_read_data inode_number %d, len %zu, offset %zu.\n", inode_number, len, offset);

    struct inode in;
    if (read_inode(image_path, inode_number, &in) != 0) {
        fprintf(stderr, "Error al leer el inodo %d\n", inode_number);
        return -1;
    }

    if (offset >= in.size) {
        fprintf(stderr, "Offset fuera del tamaño del archivo\n");
        return -1;
    }

    // Ajustar len si la lectura se pasaría del tamaño real del archivo
    if (offset + len > in.size) {
        len = in.size - offset;
        DEBUG_PRINT("Ajustando longitud de lectura a %zu bytes.\n", len);
    }

    uint8_t block_buf[BLOCK_SIZE];
    uint8_t *dst = (uint8_t *)data_buf;
    size_t remaining = len;

    size_t start_block = offset / BLOCK_SIZE;
    size_t start_offset = offset % BLOCK_SIZE;

    for (size_t i = start_block; remaining > 0; i++) {
        int block_num = get_block_number_at(image_path, &in, i);
        if (block_num <= 0) {
            fprintf(stderr, "Error inesperado obteniendo el bloque número %zu del archivo\n", i);
            return -1;
        }

        if (read_block(image_path, block_num, block_buf) != 0) {
            fprintf(stderr, "Error leyendo bloque %d\n", block_num);
            return -1;
        }

        size_t read_offset = (i == start_block) ? start_offset : 0;
        size_t space = BLOCK_SIZE - read_offset;
        size_t to_read = (remaining < space) ? remaining : space;

        memcpy(dst, block_buf + read_offset, to_read);

        DEBUG_PRINT("Leyendo bloque %d, Read offset %zu, space %zu, to_read %zu.\n", block_num, read_offset, space,
                    to_read);

        dst += to_read;
        remaining -= to_read;
    }

    // Actualizar solo el atime
    DEBUG_PRINT("inode_read_data: atime valor anterior %u.\n", in.atime);
    time_t now = time(NULL);
    in.atime = (uint32_t)now;
    DEBUG_PRINT("Actualizando atime del inodo %u a %u.\n", inode_number, in.atime);

    if (write_inode(image_path, inode_number, &in) != 0) {
        fprintf(stderr, "Error al actualizar el atime del inodo %u.\n", inode_number);
        return -1;
    }

    return len;
}
