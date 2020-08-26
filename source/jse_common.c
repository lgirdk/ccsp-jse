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
                char * newbuf = NULL;

                size *= 2;

                newbuf = (char*)realloc(buffer, size);
                if (newbuf == NULL)
                {
                    JSE_ERROR("realloc() failed: %s", strerror(errno));
                    bytes = -1;
                }
                else
                {
                    buffer = newbuf;
                }
            }
        }
        else
        {
            size = off;

            /* Do NOT resize to zero bytes (as that's a free) */
            if (size > 0)
            {
                char * newbuf = NULL;

                /* Resize the string for the contents to fit. */
                newbuf = (char*)realloc(buffer, size);
                if (newbuf == NULL)
                {
                    JSE_ERROR("realloc() failed: %s", strerror(errno));
                    bytes = -1;
                }
                else
                {
                    buffer = newbuf;
                }
            }
        }
    }

    if (bytes != -1)
    {
        *pbuffer = buffer;
        *poff = off;
        *psize = size;

        JSE_VERBOSE("buffer=%p, off=%u, size=%u", buffer, off, size)
    }

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
        size = -1;
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
            JSE_ERROR("jse_read_fd_once() failed: %s", strerror(errno))
            free(buffer);
            size = -1;
        }
        else
        {
            *pbuffer = buffer;
            *psize = size;
        }
    }

    return size;
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

    JSE_ENTER("jse_read_file(\"%s\", %p, %p)", filename, pbuffer, psize)

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

    JSE_EXIT("jse_read_file()=%d", size)
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
    JSE_EXIT("jse_read_file()=-1")
    return -1;
}

/**
 * Creates a sub directory and all the intermediate directories.
 *
 * Creates a sub directory and all the intermediate directories. If the
 * directory currently exists that is not an error. If a file exists with
 * the same name, or any elements in the path are not a directory, that
 * is an error. Errors also occur due to permissions, read only file
 * systems and so on.
 *
 * @param path the sub directory path.
 *
 * @return 0 on success or -1 on error.
 */
int jse_mkdir(const char* path)
{
    int ret = -1;

    // NULL pointer is an error
    if (path == NULL)
    {
        JSE_ERROR("path is NULL!")
    }
    else
    // If must be an absolute path.
    if (path[0] != '/')
    {
        JSE_ERROR("Invalid path: %s", path);
    }
    else
    {
        struct stat st = { 0 };

        // Test the path. If it errors, try and make it.
        if (stat(path, &st) != 0)
        {
            char* tmp = NULL;

            JSE_INFO("stat(): %s", strerror(errno))

            tmp = strdup(path);
            if (tmp != NULL)
            {
                bool err = false;
                char * p = NULL;

                // Make all the intermediate directories
                for (p = tmp + 1; *p && !err; p++)
                {
                    if (*p == '/')
                    {
                        *p = 0;
                        JSE_VERBOSE("mkdir(\"%s\", S_IRWXU)", tmp)
                        if (mkdir(tmp, S_IRWXU) != 0)
                        {
                            switch (errno)
                            {
                                // Allowed errors
                                case EROFS:
                                case EEXIST:
                                    JSE_INFO("mkdir(): %s", strerror(errno))
                                    break;
                                default:
                                    JSE_ERROR("mkdir(): %s", strerror(errno))
                                    err = true;
                                    break;
                            }

                        }
                        *p = '/';
                    }
                }

                // Make the final directory
                if (!err)
                {
                    JSE_VERBOSE("mkdir(\"%s\", S_IRWXU)", tmp)
                    if (mkdir(tmp, S_IRWXU) != 0)
                    {
                        // Existing is allowed
                        if (errno != EEXIST)
                        {
                            JSE_ERROR("mkdir(): %s", strerror(errno))
                        }
                        else
                        {
                            JSE_INFO("mkdir(): %s", strerror(errno))
                            ret = 0;
                        }
                    }
                    else
                    {
                        ret = 0;
                    }
                }

                free(tmp);
            }
            else
            {
                JSE_ERROR("strdup() failed: %s", strerror(errno))
            }
        }
        else
        {
            // Path exists. Is it a directory?
            if (S_ISDIR(st.st_mode))
            {
                ret = 0;
            }
            else
            {
                JSE_ERROR("%s: not a directory", path);
            }
        }
    }

    return ret;
}
