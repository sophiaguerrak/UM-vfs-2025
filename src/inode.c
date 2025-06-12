// inode.c

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "vfs.h"

int read_inode(const char *image_path, uint32_t inode_number, struct inode *in) {
    // Lee nodo-I de la posicion inode_number
    // lo retorna en la estructura apuntada por *in
    // Retorna 0 o -1 si encuentra un error

    struct superblock sb_struct, *sb = &sb_struct;

    // Leer el superbloque
    if (read_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al leer superblock\n");
        return -1;
    }

    if (inode_number < ROOTDIR_INODE || inode_number >= sb->inode_count) {
        fprintf(stderr, "Error en read_inode: nro nodo-I inválido (%d)\n", inode_number);
        return -1;
    }

    int block_index = inode_number / INODES_PER_BLOCK;
    int block_offset = inode_number % INODES_PER_BLOCK;

    // Leer el bloque de inodos correspondiente
    uint8_t inode_block_buffer[BLOCK_SIZE];
    if (read_block(image_path, sb->inode_start + block_index, inode_block_buffer) != 0)
        return -1;

    // Obtener el puntero al inodo dentro del bloque
    struct inode *inodes = (struct inode *)inode_block_buffer;
    *in = inodes[block_offset];

    return 0;
}

int write_inode(const char *image_path, uint32_t inode_number, const struct inode *in) {
    struct superblock sb_struct, *sb = &sb_struct;
    // Escribe nodo-I de la posicion inode_number
    // lo lee de la estructura apuntada por *in
    // Retorna 0 o -1 si encuentra un error

    // Leer el superbloque
    if (read_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al leer superblock\n");
        return -1;
    }

    if (inode_number < ROOTDIR_INODE || inode_number >= sb->inode_count) {
        fprintf(stderr, "Error en write_inode: nro nodo-I inválido (%d)\n", inode_number);
        return -1;
    }

    int block_index = inode_number / INODES_PER_BLOCK;
    int block_offset = inode_number % INODES_PER_BLOCK;

    // Leer el bloque de inodos correspondiente
    uint8_t inode_block_buffer[BLOCK_SIZE];
    if (read_block(image_path, sb->inode_start + block_index, inode_block_buffer) != 0)
        return -1;

    // Modificar el inodo en memoria
    struct inode *inodes = (struct inode *)inode_block_buffer;
    inodes[block_offset] = *in;

    // Escribir el bloque modificado en disco
    if (write_block(image_path, sb->inode_start + block_index, inode_block_buffer) != 0)
        return -1;

    DEBUG_PRINT("Inodo %u escrito correctamente en bloque %u, offset %u\n", inode_number, sb->inode_start + block_index,
                block_offset);

    return 0;
}

int free_inode(const char *image_path, uint32_t inode_number) {
    // Libera un (supuestamente ocupado) nodo-I
    // retorna 0 si lo hace, -1 si encuentra un error
    struct superblock sb_struct, *sb = &sb_struct;

    // Leer el superbloque
    if (read_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al leer superblock\n");
        return -1;
    }

    if (inode_number <= ROOTDIR_INODE || inode_number >= sb->inode_count) {
        fprintf(stderr, "Error en write_inode: rno nodo-I inválido (%d)\n", inode_number);
        return -1;
    }

    struct inode in;
    if (read_inode(image_path, inode_number, &in) != 0) {
        fprintf(stderr, "Error al leer nodo-I nro %d\n", inode_number);
        return -1;
    }

    if (in.mode == 0) {
        DEBUG_PRINT("Advertencia: nodo-I %d ya estaba libre\n", inode_number);
        return 0;
    }

    // Marcar nodo-I como libre
    memset(&in, 0, sizeof(struct inode));

    if (write_inode(image_path, inode_number, &in) != 0) {
        fprintf(stderr, "Error al escribir nodo-I liberado %d\n", inode_number);
        return -1;
    }

    sb->free_inodes++;

    if (write_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al actualizar superbloque\n");
        return -1;
    }

    DEBUG_PRINT("Nodo-I %d liberado\n", inode_number);
    return 0;
}

int get_block_number_at(const char *image_path, struct inode *in, uint16_t index) {
    // funcion prevista para ir "avanzando" bloque a bloque al procesar un archivo
    // retorna el nro de bloque de la posicion index (0, 1, ...) asociado al inode *in
    // recorre primero los directos, luego los indirectos
    // retorna -1 si encuentra un error, o 0 si index esta fuera de rango

    if (index >= in->blocks) {
        DEBUG_PRINT("index %u >= in->blocks %u\n", index, in->blocks);
        return 0; // No es un error, tal vez fue mal invocada
    }

    if (index < NUM_DIRECT_PTRS) {
        // Acceso a puntero directo
        return in->direct[index];
    } else {
        // Acceso al bloque indirecto
        uint8_t buffer[BLOCK_SIZE];
        if (in->indirect == 0) {
            fprintf(stderr, "Error: bloque indirecto es 0, con index %d y in->blocks %u\n", index, in->blocks);
            return -1;
        }

        if (read_block(image_path, in->indirect, buffer) != 0) {
            fprintf(stderr, "Error al leer el bloque indirecto %u: %s\n", in->indirect, strerror(errno));
            return -1;
        }

        struct indirect_block *ind = (struct indirect_block *)buffer;
        uint16_t indirect_index = index - NUM_DIRECT_PTRS;

        if (indirect_index >= NUM_INDIRECT_PTRS) {
            fprintf(stderr, "Error inesperado. indirect_index %d es mayor que %zu\n", indirect_index,
                    NUM_INDIRECT_PTRS);
            return -1;
        }

        return ind->blocks[indirect_index];
    }
}

int create_empty_file_in_free_inode(const char *image_path, uint16_t perms) {
    // Busca un nodo-I vacio para un archivo nuevo, inicialmente sin datos
    // Pone valores iniciales en el nodo-I
    // El unico valor que acepta como argumento para el nodo-I son los permisos perms
    // Retorna el nro de nodo-I utilizado, o -1 en caso de error

    struct superblock sb_struct, *sb = &sb_struct;

    // Leer el superbloque
    if (read_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al leer superblock\n");
        return -1;
    }

    // Verificar si hay inodos libres
    if (sb->free_inodes == 0) {
        errno = ENOSPC;
        return -1;
    }

    // Buscar un inodo libre
    for (uint32_t inode_nbr = ROOTDIR_INODE + 1; inode_nbr < sb->inode_count; inode_nbr++) {
        struct inode tmp_inode;

        if (read_inode(image_path, inode_nbr, &tmp_inode) != 0)
            return -1;
        if (tmp_inode.mode == 0) { // inodo libre
            DEBUG_PRINT("Encontrado nodo-I libre nro %u.\n", inode_nbr);

            // Inicializar y luego escribir el inodo con valores por defecto, excepto perms
            struct inode in_struct = {0}, *in = &in_struct;
            in->mode = INODE_MODE_FILE | perms;
            in->uid = getuid();
            in->gid = getgid();
            in->blocks = 0;
            in->size = 0;

            time_t now = time(NULL);
            in->atime = in->mtime = in->ctime = (uint32_t)now;

            if (write_inode(image_path, inode_nbr, in) != 0) {
                fprintf(stderr, "Error al escribir nodo-I nro %u.\n", inode_nbr);
                return -1;
            }

            // Actualiza y reescribe superbloque
            sb->free_inodes--;

            if (write_superblock(image_path, sb) != 0) {
                fprintf(stderr, "Error: no se pudo escribir el superbloque\n");
                return -1;
            }

            return inode_nbr;
        }
    }

    // si llegamos a este punto sin retornar, es que no hay mas nodos-I libres
    errno = ENOSPC;
    return -1;
}

int inode_append_block(const char *image_path, struct inode *in, uint32_t new_block_number) {
    // Agrega bloque nro new_block_number al final de los bloques del archivo
    // Retorna 0 si ejecuta bien, o -1 en caso de error
    // Es responsabilidad del llamador
    //      1. escribir a disco el nodo-I actualizado
    //      2. que new_block_number sea un bloque valido sin usar, pero marcado ocupado en el bitmap

    struct superblock sb_struct, *sb = &sb_struct;

    // Leer el superbloque para validar argumentos
    if (read_superblock(image_path, sb) != 0) {
        fprintf(stderr, "Error al leer superblock\n");
        return -1;
    }

    if (new_block_number < sb->data_start || new_block_number >= sb->total_blocks) {
        fprintf(stderr, "Bloque %u fuera de rango para agregar a archivo.\n", new_block_number);
        return -1;
    }

    // Verificamos espacio en los punteros directos
    for (int i = 0; i < NUM_DIRECT_PTRS; i++) {
        if (in->direct[i] == 0) { // encontro un puntero directo libre
            in->direct[i] = new_block_number;
            in->blocks++;
            return 0;
        }
    }

    // Bloques directos ocupados, vamos a los indirectos
    // Si ya esta usado el bloque indirecto, lo leemos; si no, lo inicializamos
    uint32_t indirect_block[NUM_INDIRECT_PTRS] = {0}; // inicializado en 0

    if (in->indirect == 0) {
        // indirecto NO Existe: Asignamos nuevo bloque para punteros indirectos
        int indirect_block_num = bitmap_set_first_free(image_path);
        if (indirect_block_num == -1) {
            fprintf(stderr, "No hay bloques disponibles para el bloque indirecto\n");
            return -1;
        }

        in->indirect = indirect_block_num;

    } else {
        // EXISTE: Leemos el bloque indirecto existente
        if (read_block(image_path, in->indirect, indirect_block) != 0) {
            fprintf(stderr, "Error leyendo el bloque indirecto nro. %u\n", in->indirect);
            return -1;
        }
    }

    // Buscar una posición libre en el bloque indirecto
    for (uint32_t i = 0; i < NUM_INDIRECT_PTRS; i++) {
        if (indirect_block[i] == 0) { // buscamos al primer bloque en 0
            indirect_block[i] = new_block_number;

            // Escribir a "disco" el bloque indirecto actualizado
            if (write_block(image_path, in->indirect, indirect_block) != 0) {
                fprintf(stderr, "Error escribiendo el bloque indirecto nro. %u\n", in->indirect);
                return -1;
            }

            in->blocks++;
            return 0;
        }
    }

    // No hay espacio ni en directos ni en indirectos
    fprintf(stderr, "Error: El archivo ha alcanzado el límite de bloques\n");
    return -1;
}

int inode_trunc_data(const char *image_path, struct inode *in) {
    // Elimina todos los bloques de datos del archivo,
    // marcandolos como libres en el bitmap y actualizando indirectamente el superblock
    // Retorna 0 si ejecuta bien, o -1 en caso de error

    // Liberar bloques directos
    for (int i = 0; i < NUM_DIRECT_PTRS; i++) {
        if (in->direct[i] != 0) {
            DEBUG_PRINT("Liberando bloque directo #%d: %u\n", i, in->direct[i]);
            bitmap_free_block(image_path, in->direct[i]);
            in->direct[i] = 0;
        }
    }

    // Liberar bloques indirectos, a partir del bloque de punteros indirectos
    if (in->indirect != 0) {
        DEBUG_PRINT("Leyendo bloque indirecto: %u\n", in->indirect);

        uint32_t indirect_block[NUM_INDIRECT_PTRS];
        if (read_block(image_path, in->indirect, indirect_block) != 0) {
            fprintf(stderr, "Error al leer bloque indirecto nro %u.\n", in->indirect);
            return -1;
        } else {
            for (size_t j = 0; j < NUM_INDIRECT_PTRS; j++) {
                if (indirect_block[j] != 0) {
                    DEBUG_PRINT("Liberando bloque referenciado indirecto #%zu: %u\n", j, indirect_block[j]);
                    bitmap_free_block(image_path, indirect_block[j]);
                }
            }
        }

        DEBUG_PRINT("Liberando bloque de punteros indirectos: %u\n", in->indirect);
        bitmap_free_block(image_path, in->indirect);
        in->indirect = 0;
    }

    DEBUG_PRINT("Archivo truncado: tamaño y bloques puestos en cero\n");

    in->size = 0;
    in->blocks = 0;
    time_t now = time(NULL);
    in->mtime = in->atime = now;

    return 0;
}