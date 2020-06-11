#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "jse_debug.h"
#include "jse_common.h"

/**
 * Create a new JSE context.
 *
 * @param filename the script filename.
 * @return the context or NULL on error.
 */
jse_context_t *jse_context_create(char* filename)
{
    jse_context_t *jse_ctx = (jse_context_t*)calloc(sizeof(jse_context_t), 1);
    if (jse_ctx != NULL)
    {
        jse_ctx->filename = filename;
    }

    return jse_ctx;
}

/**
 * Destroy a JSE context.
 *
 * @param jse_ctx the jse context.
 */
void jse_context_destroy(jse_context_t *jse_ctx)
{
    if (jse_ctx != NULL)
    {
        if (jse_ctx->filename != NULL)
        {
            free(jse_ctx->filename);
        }

        free(jse_ctx);
    }
    else
    {
        JSE_ERROR("Invalid arguments!")
    }
}

/**
 * Read from a file descriptor in to a string buffer resizing if needed.
 *
 * This function reads from fd updating the buffer pointed to by pbuffer.
 * The data is stored at the offset pointed to by poff. The size of the
 * buffer is in the size pointed to by psize. The string is nul character
 * terminated.
 *
 * If the buffer is resized the values pointed to by pstr, poff and psize
 * will be updated.
 *
 * If the read fails the buffer is freed and the values of pbuffer, poff and
 * psize will be set to NULL and 0 as appropriate.
 *
 * @param fd the file descriptor to read.
 * @param pbuffer a pointer to the buffer.
 * @param poff a pointer to the buffer offset.
 * @param psize a pointer to the total buffer size.
 *
 * @return the bytes read, 0 on end of file, or -1 on error.
 */
ssize_t jse_read_fd_once(int fd, char ** const pbuffer, off_t * const poff, size_t * const psize)
{
    ssize_t bytes = -1;
    char * buffer = *pbuffer;
    size_t size = *psize;
    off_t off = *poff;

    TEMP_FAILURE_RETRY(bytes = read(fd, buffer + off, size - off));
    if (bytes == -1)
    {
        JSE_ERROR("read() failed: %s", strerror(errno))

        free(buffer);
        buffer = NULL;
        off = 0;
        size = 0;
    }
    else
    {
        if (bytes > 0)
        {
            /* More data */
            off += bytes;

            /* Do we need to resize the string */
            if (off == (ssize_t)size)
            {
                size *= 2;

                buffer = (char*)realloc(buffer, size);
                if (buffer == NULL)
                {
                    JSE_ERROR("realloc() failed: %s", strerror(errno));
                    bytes = -1;
                    off = 0;
                    size = 0;
                }
            }
        }
        else
        {
            char * newbuf = NULL;

            /* For nul terminator */
            size = off + 1;

            /* Resize the string for the contents to fit. */
            newbuf = (char*)realloc(buffer, size);
            if (newbuf == NULL)
            {
                JSE_ERROR("realloc() failed: %s", strerror(errno));
                bytes = -1;
                off = 0;
                size = 0;
            }
            else
            {
                buffer = newbuf;

                /* nul character to terminate string, will be
                   overwritten next pass. */
                *(buffer + off) = '\0';
            }
        }
    }

    *pbuffer = buffer;
    *poff = off;
    *psize = size;

    return bytes;
}

/**
 * Read from a file descriptor.
 *
 * This function reads an unknown number of bytes from a file descriptor.
 * It does this by creating a buffer and resizing it as necessary so can
 * be used when seeking will not work.
 *
 * @param fd the file descriptor.
 * @param pbuffer a pointer to return the buffer.
 * @param psize a pointer to return the size.
 *
 * @return the file size or -1 on error.
 */
ssize_t jse_read_fd(int fd, char ** const pbuffer, size_t * const psize)
{
    /* Initial buffer size */
    size_t size = 4096;
    char * buffer = (char*)malloc(size);
    off_t off = 0;
    ssize_t bytes = -1;

    if (buffer == NULL)
    {
        JSE_ERROR("malloc() failed: %s", strerror(errno))
    }
    else
    {
        do
        {
            bytes = jse_read_fd_once(fd, &buffer, &off, &size);
        }
        while (bytes > 0);

        if (bytes == -1)
        {
            JSE_ERROR("jse_read_fd_once(0 failed: %s", strerror(errno))
        }
        else
        {
            *pbuffer = buffer;
            *psize = size;
        }
    }

    return bytes;
}

/**
 * Reads a file in to a buffer.
 *
 * Reads a file in to a buffer allocating the storage from the heap. The
 * variables pointed to by pbuffer and psize are updated with the buffer
 * and size on sucess. They are unchanged on failure. The buffer should
 * be freed when no longer required.
 *
 * @param filename the filename of the file.
 * @param pbuffer a pointer to return the buffer.
 * @param psize a pointer to return the size.
 *
 * @return the file size or -1 on error.
 */
ssize_t jse_read_file(const char * const filename, char ** const pbuffer, size_t * const psize)
{
    char * buffer = NULL;
    off_t offset = 0;
    size_t size = 0;
    ssize_t bytes = 0;
    int fd = -1;

    if (filename == NULL || pbuffer == NULL || psize == NULL)
    {
        JSE_ERROR("Invalid arguments!")
        goto error2;
    }

    JSE_DEBUG("Opening: %s", filename)
    fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        JSE_ERROR("%s: %s", filename, strerror(errno))
        goto error;
    }

    /* Finding size and a single malloc is more efficient all round! */
    offset = lseek(fd, 0, SEEK_END);
    if (offset == -1)
    {
        JSE_WARNING("%s: %s", filename, strerror(errno))

        /* Lets try falling back to the iterative method. */
        if (jse_read_fd(fd, &buffer, &size) == -1)
        {
            JSE_ERROR("jse_read_fd() failed: %s", strerror(errno));
            goto error;
        }
        else
        {
            goto done;
        }
    }

    size = (size_t)offset;
    JSE_DEBUG("File size: %u", size)

    if (size > JSE_MAX_FILE_SIZE)
    {
        JSE_ERROR("File too large!")
        goto error;
    }

    offset = lseek(fd, 0, SEEK_SET);
    if (offset == -1)
    {
        JSE_ERROR("%s: %s", filename, strerror(errno))
        goto error;
    }

    buffer = malloc(size + 1);
    if (buffer == NULL)
    {
        JSE_ERROR("%s: %s", filename, strerror(errno))
        goto error;
    }

    do
    {
        TEMP_FAILURE_RETRY(bytes = read(fd, buffer + offset, size - offset));
        if (bytes == -1)
        {
            JSE_ERROR("%s: %s", filename, strerror(errno))
            goto error;
        }

        if (bytes > 0)
        {
           offset += bytes;
        }
    }
    while (bytes > 0 && size - offset > 0);

    *(buffer + size) = '\0';

done:
    close(fd);

    *pbuffer = buffer;
    *psize = size;

    return size;

error:
    if (buffer != NULL)
    {
        free(buffer);
    }

    if (fd != -1)
    {
        close(fd);
    }

error2:
    return -1;
}


