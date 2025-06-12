# Proyecto de Filesystem VFS - UM - SSOO Generación 2025

## Descripción General

Se propone a los estudiantes implementar nueva funcionalidad a un sistema de archivos simplificado, inspirado en conceptos de nodos-I y directorios de Unix/Linux. El objetivo es desarrollar, a partir de cierto código proporcionado por los profesores, una aplicación que permita crear y manipular una imagen de disco en memoria.

Este proyecto busca reforzar conceptos de programación en C, sistemas de archivos, manejo de estructuras, acceso a bajo nivel y modularización del código en C.

### Organización de la entrega

La entrega del trabajo debe incluir:

* Un archivo fuente `.c` para **cada comando** implementado (`vfs-mkfs.c`, `vfs-ls.c`, `vfs-copy.c`, `vfs-rm.c`, `vfs-touch.c`, `vfs-cat.c`, etc.).
* El archivo de headers común (`vfs.h`) con todas las estructuras, constantes y prototipos utilizados.
* Un archivo `Makefile` que compile todos los comandos. Puede usarse el proporcionado, o cambiarlo a su criterio.
* Archivos con funciones auxiliares (por ejemplo: `inode.c`, `bitmap.c`, etc.), que ya fueron proporcionadas, pero puede agregar más a su criterio.

La estructura del proyecto debe permitir compilar todos los comandos con un solo `make` y generar ejecutables separados para cada utilidad.

---

## Parámetros de Formato

* Tamaño de bloque: **1024 bytes fijo** (para simplificar el diseño y la implementación).
* El primer bloque siempre contiene el **superbloque**.
* El segundo bloque en adelante contiene la **tabla de nodos-i**.
* Luego sigue el **bitmap de bloques**.
* Luego siguen los **bloques de datos**.
* El **nodo-i 0** no se usa, ya que una entrada de directorio que apunte a 0 se considera sin usar.
* El **nodo-i 1** corresponde al directorio raíz (único directorio), que debe contener las entradas especiales `.` y `..` desde su creación.

---

## Estructuras y constantes

* Deben usarse las estructuras y constantes ya definidas en `include/vfs.h`.
* Pueden agregarse otras, pero no modificar las que ya existen.

---

## Funciones auxiliares proporcionadas

Este proyecto incluye un conjunto de funciones auxiliares ya implementadas que permiten gestionar internamente las estructuras del sistema de archivos virtual. Estas funciones no deben ser modificadas. Su uso es esencial para interactuar correctamente con la imagen del sistema de archivos.

### Manejo de bloques (read-write-block.c)

Son las funciones de más bajo nivel, que simulan leer y escribir bloques en un disco físico.

* `int read_block(const char *image_path, int block_number, void *buffer)`

  * Lee un bloque de la imagen al buffer dado. Retorna 0 en éxito, -1 en error.

* `int write_block(const char *image_path, int block_number, const void *buffer)`

  * Escribe un bloque desde el buffer dado. Retorna 0 en éxito, -1 en error.

* `int create_block_device(const char *image_path, int total_blocks, int block_size)`

  * Crea un archivo vacío del tamaño deseado, inicializado en ceros. Retorna 0 o -1.

### Bitmap (bitmap.c)

* `int bitmap_set_first_free(const char *image_path)`

  * Encuentra el primer bloque libre en el bitmap, lo marca como ocupado y retorna su número. Retorna -1 si no hay bloques.

* `int bitmap_free_block(const char *image_path, uint32_t block_nbr)`

  * Marca como libre un bloque previamente asignado, escribiendo ceros. Retorna 0 o -1 en error.

* `void print_bitmap_block(uint8_t *buffer, uint32_t size)`

  * Imprime en consola una representación visual del bitmap del filesystem.

### Superbloque (superblock.c)

* `int read_superblock(const char *image_path, struct superblock *sb)`

  * Carga el superbloque desde disco.

* `int write_superblock(const char *image_path, struct superblock *sb)`

  * Escribe el superbloque a disco.

* `int init_superblock(const char *image_path, uint32_t total_blocks, uint32_t total_inodes)`

  * Inicializa los valores del superbloque y actualiza el bitmap.

* `void print_superblock(const struct superblock *sb)`

  * Muestra por consola el contenido del superbloque.

### Inodos (inode.c)

* `int read_inode(const char *image_path, uint32_t inode_number, struct inode *in)`

  * Lee un nodo-i de disco.

* `int write_inode(const char *image_path, uint32_t inode_number, const struct inode *in)`

  * Escribe un nodo-i en disco.

* `int free_inode(const char *image_path, uint32_t inode_number)`

  * Libera un nodo-i, marcándolo como vacío.

* `int create_empty_file_in_free_inode(const char *image_path, uint16_t perms)`

  * Reserva un nodo-i vacío y lo inicializa con los permisos dados.

* `int inode_append_block(const char *image_path, struct inode *in, uint32_t new_block_number)`

  * Agrega un bloque al final del archivo representado por el nodo-i.

* `int inode_trunc_data(const char *image_path, struct inode *in)`

  * Elimina todos los bloques de datos del archivo.

* `int get_block_number_at(const char *image_path, struct inode *in, uint16_t index)`

  * Devuelve el número de bloque en la posición dada (acceso directo o indirecto).

### Datos de archivos (read-write-data.c)

* `int inode_write_data(const char *image_path, uint32_t inode_number, void *buffer, size_t len, size_t offset)`

  * Escribe datos en el archivo, desde el _buffer_, siendo _len_ la cantidad de bytes a escribir y a partir de qué posición (_offset_) del archivo, gestionando asignación de bloques si es necesario.

* `int inode_read_data(const char *image_path, uint32_t inode_number, void *buffer, size_t len, size_t offset)`

  * Lee datos desde un archivo a partir de un _offset_, cargando _len_ bytes en el _buffer_.

### Directorio raíz y entradas (rootdir.c)

* `int create_root_dir(const char *image_path)`

  * Crea e inicializa el directorio raíz con entradas `.` y `..`.

### Utilidades de formato y directorio (ls-func.c)

* `const char *str_file_type(uint16_t mode)`

  * Retorna el tipo de archivo en formato Unix (`-` o `d`).

* `const char *str_file_permissions(uint16_t mode)`

  * Convierte los permisos en modo Unix (`rwxr-xr--`).

* `const char *str_user(uint16_t uid)`

  * Devuelve el nombre del usuario a partir del UID.

* `const char *str_group(uint16_t gid)`

  * Devuelve el nombre del grupo a partir del GID.

* `void str_timestamp(uint32_t ts, char *buffer, size_t size)`

  * Convierte un timestamp a string legible (formato YYYY-MM-DD HH\:MM\:SS).

* `void print_inode(const struct inode *in, uint32_t inode_nbr, const char *filename)`

  * Muestra los datos de un nodo-i formateados, estilo `ls -l`.

* `int name_is_valid(const char *name)`

  * Valida nombres de archivo según reglas del filesystem.

* `int dir_lookup(const char *image_path, const char *filename)`

  * Busca un archivo en el directorio raíz.

* `int add_dir_entry(const char *image_path, const char *filename, uint32_t inode_number)`

  * Agrega una nueva entrada al directorio raíz.

* `int remove_dir_entry(const char *image_path, const char *filename)`

  * Elimina una entrada del directorio raíz.

---

Estas funciones deben ser utilizadas como base para implementar los comandos restantes del sistema de archivos virtual.

## Comandos del sistema ya proporcionados

### `vfs-mkfs`

```bash
vfs-mkfs imagen cantidad_bloques cantidad_inodos
```

* El archivo `imagen` **no debe existir previamente**.
* Crea la imagen vacía, inicializando el superbloque, la tabla de nodos-i, el bitmap y el bloque del directorio raíz.
* El superbloque debe "firmarse" con el número `MAGIC_NUMBER`.
* El bloque 0 será el superbloque.
* En el bloque 1 comienzan los bloques con los nodos-i.
* Luego de los bloques de nodos-I están los bloques del mapa de bits o `bitmap` que marca como libres u ocupados todos los bloques del filesystem
* A continuación, irán los bloques de datos, el primero de los cuales tendrá el primer bloque de datos del directorio raíz.


### `vfs-info`

```bash
vfs-info imagen
```

* Muestra información general del filesystem:

  * Contenido del superbloque (todos sus campos)
  * Cantidad de bloques libres y ocupados
  * Muestra el bitmap de bloques de forma textual, indicando con `.` los bloques libres y con `#` los bloques usados (en filas de 64 bloques)

### `vfs-copy`

```bash
vfs-copy imagen archivo_origen nombre_destino
```

* Copia un archivo del sistema anfitrión al filesystem.
* El nombre de destino debe cumplir las restricciones de nombres: letras, números, `.`, `_`, `-`.
* Si no hay espacio suficiente, debe abortar informando el error.



## Comandos del sistema a desarrollar


### `vfs-touch`

```bash
vfs-touch imagen archivo1 [archivo2...]
```

* Crea archivos vacíos.
* Si el nombre ya existe, debe rechazarlo.

### `vfs-ls`

```bash
vfs-ls imagen
```

* Muestra una lista al estilo `ls -l`, sin ordenar, incluyendo:

  * Nombre
  * Número de nodo-I
  * Usuario
  * Grupo
  * Tamaño en bytes
  * Cantidad de bloques
  * Tipo
  * Fechas (formato legible)


### `vfs-lsort`

```bash
vfs-lsort imagen
```

* Similar a la anterior `vfs-ls`, pero ordenada _alfabéticamente por nombre de archivo_.



### `vfs-cat`

```bash
vfs-cat imagen archivo1 [archivo2...]
```

* Muestra por salida estándar el contenido de uno o más archivos concatenados.

### `vfs-trunc`

```bash
vfs-trunc imagen archivo1 [archivo2...]
```

* Elimina el contenido de uno o más archivos.
* El archivo sigue existiendo, pero con tamaño 0 y sin bloques asignados.
* Solo se pueden borrar archivos regulares.

### `vfs-rm`

```bash
vfs-rm imagen archivo1 [archivo2...]
```

* Borra uno o más archivos.
* Solo se pueden borrar archivos regulares.


## Aprendizajes esperados

A través de este trabajo, los estudiantes deberán comprender y poder responder a las siguientes preguntas, entre otras:

* *¿Qué significa compilar un programa en C?*
* *¿Qué son los archivos `.o`?*
* *¿Qué son los archivos `.h`?*
* *¿Cuál es el propósito de las sentencias `#include` y `#define`?*
* *¿Para qué sirve un Makefile?*
* *Indique algunas flags útiles del compilador GCC*
* *¿Cuál es la diferencia entre `printf`, `fprintf` y `DEBUG_PRINT` en este proyecto?*
* *¿Qué es un puntero en C y cómo se usa?*
* *¿Qué significa recibir punteros como argumentos en una función?*
* *¿Cómo se gestionan errores en C? ¿Qué es `errno`?*
* *¿Qué significa EXIT_SUCCESS o EXIT_FAILURE y cómo se usa?*
* *¿Qué implica que una variable local sea `static`?*
* *¿Qué implica que una función sea `static` en un archivo fuente?*
* *¿Qué es `memcpy` y para qué se usa?*
* *¿Qué hacen las llamadas al sistema `read()`, `write()`, `lseek()` y cómo se usan en este proyecto?*
* *¿Qué otras llamadas al sistema se usan en este proyecto?*
* *¿Qué es un sistema de archivos y qué componentes lo forman?*
* *¿Qué es un inodo y qué información contiene?*
* *¿Qué rol cumple un directorio en un sistema de archivos?*
* *¿Qué implica la selección del tamaño del bloque de datos en un filesystem?*
* *¿Cómo funcionan el acceso a bloques indirectos en un inodo?*

