#define _GNU_SOURCE
#include "utils/direct_io.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __APPLE__
#define O_DIRECT 0
#endif

/* Sector size for O_DIRECT alignment. Usually 512 or 4096. 4096 is safe for both. */
#define DIO_ALIGN 4096

void
dio_buffer_init(DirectIOBuffer *dio, size_t size)
{
    if (dio->initialized)
        return;

    /* size must be aligned to DIO_ALIGN for O_DIRECT */
    dio->buffer_size = (size + (DIO_ALIGN - 1)) & ~(DIO_ALIGN - 1);
    dio->cached_start = 0;
    dio->cached_end = 0;
    dio->fd = -1;

    if (posix_memalign((void **)&dio->buffer, DIO_ALIGN, dio->buffer_size) != 0)
    {
        elog(ERROR, "posix_memalign failed: %s", strerror(errno));
    }

    dio->initialized = true;
}

int
dio_open_file(const char *path, int flags, mode_t mode)
{
    int fd;
    int open_flags = flags;

#ifndef __APPLE__
    open_flags |= O_DIRECT;
#endif

    fd = open(path, open_flags, mode);

#ifdef __APPLE__
    if (fd != -1)
    {
        if (fcntl(fd, F_NOCACHE, 1) == -1)
        {
             /* Not fatal, but good to know */
             elog(WARNING, "fcntl(F_NOCACHE) failed for %s: %s", path, strerror(errno));
        }
    }
#endif

    return fd;
}

ssize_t
dio_buffer_read_block(DirectIOBuffer *dio, char *target, off_t offset, size_t size)
{
    off_t chunk_start;
    ssize_t read_bytes;

    if (!dio->initialized)
        elog(ERROR, "DirectIOBuffer not initialized");

    if (dio->fd == -1)
        elog(ERROR, "DirectIOBuffer file descriptor not set");

    /* Check if the requested block is already in the cache */
    if (offset >= dio->cached_start && offset + size <= dio->cached_end)
    {
        memcpy(target, dio->buffer + (offset - dio->cached_start), size);
        return size;
    }

    /* Block not in cache, read new chunk */
    /* Align to DIO_ALIGN (page size / sector size) for O_DIRECT */
    dio->cached_start = (offset / DIO_ALIGN) * DIO_ALIGN;
    chunk_start = dio->cached_start;

    /* 
     * Try reading with O_DIRECT (already set on fd if possible).
     * If offset or size are not aligned, pread might return EINVAL.
     */
    read_bytes = pread(dio->fd, dio->buffer, dio->buffer_size, chunk_start);
    
    /* Fallback if O_DIRECT failed due to alignment or other reasons */
    if (read_bytes < 0 && (errno == EINVAL || errno == EOPNOTSUPP))
    {
        int flags = fcntl(dio->fd, F_GETFL);
        if (flags != -1 && (flags & O_DIRECT))
        {
            /* Temporary disable O_DIRECT for this read */
            if (fcntl(dio->fd, F_SETFL, flags & ~O_DIRECT) != -1)
            {
                read_bytes = pread(dio->fd, dio->buffer, dio->buffer_size, chunk_start);
                /* Restore O_DIRECT flags */
                fcntl(dio->fd, F_SETFL, flags);
            }
        }
    }

    if (read_bytes < 0)
        return -1;

    dio->cached_end = chunk_start + read_bytes;

    /* If we read enough to cover the requested block */
    if (offset >= dio->cached_start && offset + size <= dio->cached_end)
    {
        memcpy(target, dio->buffer + (offset - dio->cached_start), size);
        return size;
    }

    /* We might have reached EOF and read less than requested */
    if (offset < dio->cached_end)
    {
        size_t available = dio->cached_end - offset;
        size_t to_copy = (available < size) ? available : size;
        memcpy(target, dio->buffer + (offset - dio->cached_start), to_copy);
        
        /* 
         * For PostgreSQL blocks (BLCKSZ), we often need the full block.
         * If we didn't read enough, and it's not EOF, we might need to try harder.
         * But dio_buffer_read_block is usually called for BLCKSZ which is 8KB,
         * and buffer_size is 1MB, so we should have it.
         * If we reached EOF, returning less than size is correct.
         */
        return to_copy;
    }

    return 0; /* EOF */
}

void
dio_buffer_cleanup(DirectIOBuffer *dio)
{
    if (!dio->initialized)
        return;

    if (dio->buffer)
        free(dio->buffer);

    if (dio->fd != -1)
    {
        close(dio->fd);
        dio->fd = -1;
    }

    dio->buffer = NULL;
    dio->initialized = false;
}
