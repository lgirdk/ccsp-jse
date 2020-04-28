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
        goto error;
    }

    /* Finding size and a single malloc is more efficient all round! */

    offset = lseek(fd, 0, SEEK_END);
    if (offset == -1)
    {
        goto error;
    }

    size = (size_t)offset;
    JSE_DEBUG("File size: %u", size)

    if (size > JSE_MAX_FILE_SIZE)
    {
        goto error;
    }

    offset = lseek(fd, 0, SEEK_SET);
    if (offset == -1)
    {
        goto error;
    }

    buffer = malloc(size + 1);
    if (buffer == NULL)
    {
        goto error;
    }

    do
    {
        TEMP_FAILURE_RETRY(bytes = read(fd, buffer + offset, size - offset));
        if (bytes == -1)
        {
            goto error;
        }

        if (bytes > 0)
        {
           offset += bytes;
        }
    }
    while (bytes > 0 && size - offset > 0);

    *(buffer + size) = '\0';
    close(fd);

    *pbuffer = buffer;
    *psize = size;

    return size;

error:
    JSE_ERROR("Error: \"%s\" Filename: \"%s\"", strerror(errno), filename)

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


