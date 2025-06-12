// ls-func.c

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vfs.h"

// Retorna tipo estilo Unix: "-" para archivos regulares, "d" para directorios
const char *str_file_type(uint16_t mode) {
    if ((mode & INODE_MODE_DIR) == INODE_MODE_DIR)
        return "d";
    else if ((mode & INODE_MODE_FILE) == INODE_MODE_FILE)
        return "-";
    else
        return "?";
}

// Retorna puntero a string de permisos estilo Unix: rwxr-xr-x
const char *str_file_permissions(uint16_t mode) {
    static char perms[10];
    const char rwxrwxrwx[] = "rwxrwxrwx";

    for (int i = 0; i < 9; i++)
        perms[i] = (mode & (1 << (8 - i))) ? rwxrwxrwx[i] : '-';

    perms[9] = '\0';
    return perms;
}

// Retorna el usuario si existe, o de lo contrario el uid numerico
const char *str_user(uint16_t uid) {
    static char uidbuf[32];
    struct passwd *pw = getpwuid(uid);
    if (pw)
        return pw->pw_name;
    snprintf(uidbuf, sizeof(uidbuf), "%u", uid);
    return uidbuf;
}

// Retorna el grupo si existe, o de lo contrario el gid numerico
const char *str_group(uint16_t gid) {
    static char gidbuf[32];
    struct group *gr = getgrgid(gid);
    if (gr)
        return gr->gr_name;
    snprintf(gidbuf, sizeof(gidbuf), "%u", gid);
    return gidbuf;
}

// Retorna un timestamp Unix
/* NOTA:
    Las anteriores funciones str_xxxx retornaban el puntero a una memoria static
    Sin embargo, dado que str_timestamp sera usada en un printf convertir ctime, mtime y atime,
    no funcionaria el puntero a la memoria static dado que al ejecutar printf los tres
    valores estarian apuntando a los mismos datos.
    Esta implementacion es mucho mas segura, aunque mas engorrosa de usar
*/
void str_timestamp(uint32_t ts, char *buffer, size_t size) {
    DEBUG_PRINT("Entrando en str timestamp %u\n", ts);
    time_t t = ts;
    struct tm *tm_info = localtime(&t);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
    return;
}

void print_inode(const struct inode *in, uint32_t inode_nbr, const char *filename) {
    char ctime_buf[32];
    char mtime_buf[32];
    char atime_buf[32];
    str_timestamp(in->ctime, ctime_buf, sizeof(ctime_buf));
    str_timestamp(in->mtime, mtime_buf, sizeof(mtime_buf));
    str_timestamp(in->atime, atime_buf, sizeof(atime_buf));

    printf("%4u %s%s %-10s %-10s %3u %8u %s %s %s %s\n", inode_nbr, str_file_type(in->mode),
           str_file_permissions(in->mode), str_user(in->uid), str_group(in->gid), in->blocks, in->size, ctime_buf,
           mtime_buf, atime_buf, filename);
}

// Verifica que el nombre cumpla las restricciones del filesystem
// Solo letras, dígitos, '.', '_', '-' y hasta FILENAME_MAX_LEN caracteres
int name_is_valid(const char *name) {
    if (!name || strlen(name) == 0 || strlen(name) >= FILENAME_MAX_LEN)
        return 0;
    for (size_t i = 0; i < strlen(name); i++) {
        char c = name[i];
        if (!(isalnum(c) || c == '.' || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

int dir_lookup(const char *image_path, const char *filename) {
    // No valida que el nombre sea válido ni que la imagen lo sea
    // Retorna nodo-I encontrado para la entrada,
    // retorna 0 (nodo-I invalido) si no lo encuentra, o -1 en caso de errores

    struct inode root_inode;

    // Leer el nodo-I de la raiz, bien conocida
    if (read_inode(image_path, ROOTDIR_INODE, &root_inode) != 0) {
        return -1;
    }

    // recorre todos sus bloques de datos para buscar filename
    for (uint16_t i = 0; i < root_inode.blocks; i++) {

        int block_num = get_block_number_at(image_path, &root_inode, i);
        if (block_num <= 0) {
            fprintf(stderr, "Error inesperado el buscar bloque %d del directorio raiz.\n", i);
            return -1;
        }

        uint8_t data_buf[BLOCK_SIZE];
        if (read_block(image_path, block_num, data_buf) != 0) {
            return -1;
        }

        struct dir_entry *entries = (struct dir_entry *)data_buf;

        for (uint32_t j = 0; j < DIR_ENTRIES_PER_BLOCK; j++) {
            if (entries[j].inode == 0)
                continue;

            if (strncmp(entries[j].name, filename, FILENAME_MAX_LEN) == 0) {
                return entries[j].inode;
            }
        }
    }

    return 0; // No encontrado
}

int add_dir_entry(const char *image_path, const char *filename, uint32_t inode_number) {
    // No valida el nro de inodo

    if (!name_is_valid(filename)) {
        DEBUG_PRINT("Nombre de archivo %s no es valido para agregarlo al directorio.\n", filename);
        return -1;
    }
    
    struct inode root_inode;

    if (read_inode(image_path, ROOTDIR_INODE, &root_inode) != 0)
        return -1;

    for (int i = 0; i < root_inode.blocks; i++) {

        int block_num = get_block_number_at(image_path, &root_inode, i);
        if (block_num <= 0) {
            fprintf(stderr, "Error inesperado el buscar bloque %d del directorio raiz.\n", i);
            return -1;
        }

        uint8_t data_buf[BLOCK_SIZE];
        if (read_block(image_path, block_num, data_buf) != 0)
            return -1;

        struct dir_entry *entries = (struct dir_entry *)data_buf;

        for (uint32_t j = 0; j < DIR_ENTRIES_PER_BLOCK; j++) {
            if (entries[j].inode == 0) {
                entries[j].inode = inode_number;
                strncpy(entries[j].name, filename, FILENAME_MAX_LEN);
                DEBUG_PRINT("Escribiendo entry %s %u en blocknum %d.\n", filename, inode_number, block_num);

                if (write_block(image_path, block_num, data_buf) != 0)
                    return -1;

                return 0; // OK
            }
        }
    }

    // No se encontró una entrada libre
    errno = ENOSPC;
    return -1;
}

int remove_dir_entry(const char *image_path, const char *filename) {
    // elimina logicamente una entrada de directorio, escribiendo ceros en ella
    // busca la entrada por el nombre del filename
    // Retorna 0 si se eliminó o no estaba, -1 en caso de error

    struct inode root_inode;

    if (read_inode(image_path, ROOTDIR_INODE, &root_inode) != 0) {
        return -1;
    }

    for (uint16_t i = 0; i < root_inode.blocks; i++) {

        int block_num = get_block_number_at(image_path, &root_inode, i);
        if (block_num <= 0) {
            fprintf(stderr, "Error inesperado el buscar bloque %d del directorio raiz.\n", i);
            return -1;
        }

        uint8_t data_buf[BLOCK_SIZE];
        if (read_block(image_path, block_num, data_buf) != 0) {
            fprintf(stderr, "Error al leer el bloque %d: %s\n", block_num, strerror(errno));
            return -1;
        }

        struct dir_entry *entries = (struct dir_entry *)data_buf;

        for (uint32_t j = 0; j < DIR_ENTRIES_PER_BLOCK; j++) {
            if (entries[j].inode != 0 && strncmp(entries[j].name, filename, FILENAME_MAX_LEN) == 0) {

                DEBUG_PRINT("Eliminando entrada de directorio '%s' (inode %u) en bloque %d, pos %u\n", filename,
                            entries[j].inode, block_num, j);

                entries[j].inode = 0;
                memset(entries[j].name, 0, FILENAME_MAX_LEN);

                if (write_block(image_path, block_num, data_buf) != 0) {
                    fprintf(stderr, "Error al escribir bloque de directorio actualizado\n");
                    return -1;
                }

                return 0;
            }
        }
    }

    DEBUG_PRINT("Archivo '%s' no estaba en el directorio\n", filename);
    return 0; // No encontrado, pero no es error
}
