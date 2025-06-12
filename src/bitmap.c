// bitmap.c

#include "vfs.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int bitmap_free_block(const char *image_path, uint32_t block_nbr) {
    /*
        Escribe un cero en la posicion block_nbr del bitmap
        Pasos:
            Verifica que block_nbr sea válido.
            Calcula en qué bloque de bitmap se encuentra.
            Lee ese bloque de bitmap.
            Desmarca el bit correspondiente (lo pone en 0).
            Actualiza bitmap_zeroes[] y free_blocks en el superbloque.
            Escribe el bitmap y el superbloque actualizados.
    */
    struct superblock sb_struct, *sb = &sb_struct;

    if (read_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al leer superblock\n");
        return -1;
    }

    if (block_nbr <= sb->data_start || block_nbr >= sb->total_blocks) { // controla no liberar el directorio raiz
        fprintf(stderr, "Error: número de bloque inválido (%u)\n", block_nbr);
        return -1;
    }

    // Calcular en qué bloque de bitmap y en qué posición dentro de ese bloque está el bit

    uint32_t bitmap_block_offset = block_nbr / BITS_PER_BLOCK; // en que bloque está el bit
    uint32_t in_block_bit_index = block_nbr % BITS_PER_BLOCK;  // en qué número de bit de ese bloque
    uint32_t byte_index = in_block_bit_index / 8;              // en qué byte de ese bloque
    uint32_t bit_index = in_block_bit_index % 8;               // en qué bit de ese byte (izq a der)
    uint32_t bit_mask = 1 << (7 - bit_index);                  // la máscara por ej. 00100000

    // Leer el bloque de bitmap correspondiente
    uint8_t bitmap_buffer[BLOCK_SIZE];
    int bitmap_block_num = sb->bitmap_start + bitmap_block_offset;
    DEBUG_PRINT("Número de bloque del bitmap es %d.\n", bitmap_block_num);
    DEBUG_PRINT("Número de bit en el bloque bitmap es %u.\n", in_block_bit_index);

    if (read_block(image_path, bitmap_block_num, bitmap_buffer) != 0) {
        fprintf(stderr, "Error al leer bloque de bitmap %d\n", bitmap_block_num);
        return -1;
    }

    // Verificar que el bit estaba en 1
    if (!(bitmap_buffer[byte_index] & bit_mask)) {
        DEBUG_PRINT("Advertencia: el bloque %u ya estaba libre\n", block_nbr);
        return 0;
    }

    // Marcar el bit como libre
    bitmap_buffer[byte_index] &= ~bit_mask;

    // Escribir el bloque de bitmap actualizado
    if (write_block(image_path, bitmap_block_num, bitmap_buffer) != 0) {
        fprintf(stderr, "Error al escribir bloque de bitmap %d\n", bitmap_block_num);
        return -1;
    }

    // Escribir ceros en el bloque de datos liberado
    DEBUG_PRINT("Escribiendo ceros en bloque %u que quedo libre\n", block_nbr);
    uint8_t zero_buf[BLOCK_SIZE] = {0};
    if (write_block(image_path, block_nbr, zero_buf) != 0) {
        fprintf(stderr, "Error al limpiar bloque %u.\n", block_nbr);
        return -1;
    }

    // Actualizar metadata del superbloque
    sb->bitmap_zeroes[bitmap_block_offset]++;
    sb->free_blocks++;

    if (write_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al escribir superbloque\n");
        return -1;
    }

    return 0;
}

int bitmap_set_first_free(const char *image_path) {
    // Busca el primer bloque libre en el bitmap, lo marca como ocupado y lo retorna.
    // Retorna -1 en caso de error o si no hay bloques libres disponibles.

    // Paso 1: leer el superbloque (bloque 0)
    // Leer el superbloque para validar si hay bloques disponibles
    struct superblock sb_struct, *sb = &sb_struct;

    if (read_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al leer superblock\n");
        return -1;
    }

    // Paso 2: verificar si hay bloques libres
    if (sb->free_blocks == 0) {
        fprintf(stderr, "Error: no hay bloques libres\n");
        return -1;
    }

    // Paso 2: buscar un bloque del bitmap con espacio libre
    int bitmap_block_offset = -1;
    for (uint32_t i = 0; i < sb->bitmap_blocks; i++) {
        if (sb->bitmap_zeroes[i] > 0) {
            bitmap_block_offset = i;
            break;
        }
    }

    if (bitmap_block_offset == -1) {
        fprintf(stderr, "Error: inconsistencia: bitmap_zeroes no refleja bloques libres\n");
        return -1;
    }

    // Paso 3: leer el bloque de bitmap
    uint8_t bitmap_buffer[BLOCK_SIZE];
    int bitmap_block_num = sb->bitmap_start + bitmap_block_offset;
    if (read_block(image_path, bitmap_block_num, bitmap_buffer) != 0) {
        fprintf(stderr, "Error: no se pudo leer el bloque de bitmap\n");
        return -1;
    }

    // Paso 4: encontrar primer byte con algún bit libre
    int byte_index = -1;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (bitmap_buffer[i] != 0xFF) {
            byte_index = i;
            break;
        }
    }

    if (byte_index == -1) {
        fprintf(stderr, "Error: inconsistencia: bitmap parece lleno pero metadata indica espacio\n");
        return -1;
    }

    // Paso 5: encontrar primer bit libre en ese byte (de izquierda a derecha)
    int bit_index = -1;
    for (int b = 0; b < 8; b++) {
        uint8_t mask = 1 << (7 - b); // bit shift a la izquierda
        if (!(bitmap_buffer[byte_index] & mask)) {
            // Marcar el bit como ocupado
            bitmap_buffer[byte_index] |= mask;
            bit_index = b;
            break;
        }
    }

    if (bit_index == -1) {
        fprintf(stderr, "Error: inconsistencia en el byte del bitmap\n");
        return -1;
    }

    // Paso 6: calcular número de bloque, usando
    uint32_t block_number = bitmap_block_offset * BLOCK_SIZE * 8 + byte_index * 8 + bit_index;

    if (block_number >= sb->total_blocks) {
        fprintf(stderr, "Error: número de bloque fuera de rango\n");
        return -1;
    }

    // Paso 7: escribir bloque de bitmap y actualizar superbloque
    if (write_block(image_path, bitmap_block_num, bitmap_buffer) != 0) {
        fprintf(stderr, "Error: no se pudo escribir el bloque de bitmap\n");
        return -1;
    }

    sb->bitmap_zeroes[bitmap_block_offset]--;
    sb->free_blocks--;

    if (write_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error: no se pudo escribir el superbloque\n");
        return -1;
    }

    return block_number;
}

void print_bitmap_block(uint8_t *buffer, uint32_t size) {
    // Escribe el bitmap en lineas de ROW_WIDTH de ancho
    // Usa # para marcar bloque ocupado y . para libre
    
    const uint32_t ROW_WIDTH = 64;

    for (uint32_t i = 0; i < size; i++) {
        uint32_t byte_index = i / 8;
        uint32_t bit_index = i % 8;

        // Ver si el bit está en 1 (bloque ocupado)
        int bitvalue = buffer[byte_index] & (1 << (7 - bit_index));
        putchar(bitvalue ? '#' : '.');

        if ((i + 1) % ROW_WIDTH == 0 || (i + 1) == size)
            putchar('\n');
    }
}
