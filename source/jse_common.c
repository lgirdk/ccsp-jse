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
jse_context_t *jse_context_create(char *filename)
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
 * @return the file size or 0 on error.
 */
size_t jse_read_fd(int fd, char **pbuffer, size_t *psize)
{
    size_t buffer_size = 4096;
    char * buffer = (char*)malloc(buffer_size);
    size_t size = 0;
    ssize_t bytes = 0;

    if (buffer == NULL)
    {
        goto error;
    }

    do
    {
        /* Size is size of contents */
        TEMP_FAILURE_RETRY(bytes = read(fd, buffer + size, buffer_size - size));
        if (bytes == -1)
        {
            JSE_ERROR("read() failed: %s", strerror(errno))
            goto error;
        }

        if (bytes > 0)
        {
            size += bytes;

            /* Do we need to resize the buffer */
            if (size + bytes == buffer_size)
            {
                /* Can't expand it any more. */
                if (buffer_size == JSE_MAX_FILE_SIZE)
                {
                    JSE_ERROR("File too large!")
                    goto error;
                }

                buffer_size *= 2;

                if (buffer_size > JSE_MAX_FILE_SIZE)
                {
                    buffer_size = JSE_MAX_FILE_SIZE;
                }

                buffer = realloc(buffer, buffer_size);
                if (buffer == NULL)
                {
                    JSE_ERROR("realloc() failed: %s", strerror(errno))
                    goto error;
                }
            }
        }
    }
    while (bytes > 0);

    /* trim off excess memory. */
    buffer = realloc(buffer, size + sizeof('\0'));
    if (buffer == NULL)
    {
        JSE_ERROR("realloc() failed: %s", strerror(errno))
        goto error;
    }

    *(buffer + size) = '\0';

    *pbuffer = buffer;
    *psize = size;

    return size;

error:
    if (fd != -1)
    {
        close(fd);
    }

    if (buffer != NULL)
    {
        free(buffer);
    }

    return 0;
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
 * @return the file size or 0 on error.
 */
size_t jse_read_file(const char *filename, char **pbuffer, size_t *psize)
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
        if (jse_read_fd(fd, &buffer, &size) == 0)
        {
            /* That didn't work either */
            goto error;
        }
        else
        {
            close(fd);
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
    return 0;
}


