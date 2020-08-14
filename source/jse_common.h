
#ifndef JSE_COMMON_H
#define JSE_COMMON_H

#include <stdlib.h>
#include <duktape.h>
#include <qdecoder.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(exp) \
    do {} while (((long)(exp)) == -1 && errno == EINTR)
#endif

#define JSE_MAX_FILE_SIZE 100000

/* Context for processing the request. Used by the fatal error handler */
struct jse_context_s {
    char * filename;
    duk_context * ctx;
    qentry_t * req;
};

typedef struct jse_context_s jse_context_t;

/**
 * Create a new JSE context.
 *
 * @param filename the script filename.
 * @return the context or NULL on error.
 */
jse_context_t *jse_context_create(char * filename);

/**
 * Destroy a JSE context.
 *
 * @param jse_ctx the jse context.
 */
void jse_context_destroy(jse_context_t *jse_ctx);

/**
 * A single read from a file descriptor in to a buffer resizing if needed.
 *
 * This function reads from fd updating the buffer pointed to by pbuffer.
 * The data is stored at the offset pointed to by poff. The size of the
 * buffer is in the size pointed to by psize.
 *
 * If the buffer is resized the values pointed to by pstr, poff and psize
 * will be updated.
 *
 * If the read fails the buffer is freed and the values of pstr, poff and
 * psize will be set to NULL and 0 as appropriate.
 *
 * @param fd the file descriptor to read.
 * @param pbuffer a pointer to the buffer.
 * @param poff a pointer to the buffer offset.
 * @param psize a pointer to the total buffer size.
 * 
 * @return the bytes read or -1 on error.
 */
ssize_t jse_read_fd_once(int fd, char ** const pbuffer, off_t * const poff, size_t * const psize);

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
ssize_t jse_read_fd(int fd, char ** const pbuffer, size_t * const psize);

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
ssize_t jse_read_file(const char * const filename, char ** const pbuffer, size_t * const psize);

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
int jse_mkdir(const char* path);

#if defined(__cplusplus)
}
#endif

#endif
