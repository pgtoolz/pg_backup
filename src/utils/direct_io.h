#ifndef DIRECT_IO_H
#define DIRECT_IO_H

#include <sys/types.h>
#include "pg_probackup.h"

typedef struct DirectIOBuffer {
    char   *buffer;           /* aligned буфер */
    size_t  buffer_size;     /* размер буфера */
    off_t   cached_start;     /* начало кэшированных данных в файле */
    off_t   cached_end;       /* конец кэшированных данных */
    int     fd;                 /* file descriptor */
    bool    initialized;       /* флаг инициализации */
} DirectIOBuffer;

/* Инициализация буфера */
extern void dio_buffer_init(DirectIOBuffer *dio, size_t size);

/* Чтение блока через Direct I/O буфер */
extern ssize_t dio_buffer_read_block(DirectIOBuffer *dio, char *target, off_t offset, size_t size);

/* Очистка ресурсов */
extern void dio_buffer_cleanup(DirectIOBuffer *dio);

/* Открытие файла с использованием O_DIRECT или аналогов */
extern int dio_open_file(const char *path, int flags, mode_t mode);

#endif /* DIRECT_IO_H */
