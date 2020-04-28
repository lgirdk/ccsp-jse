
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
    char *filename;
    duk_context *ctx;
    qentry_t *req;
};

typedef struct jse_context_s jse_context_t;

/**
 * Create a new JSE context.
 *
 * @param filename the script filename.
 * @return the context or NULL on error.
 */
jse_context_t *jse_context_create(char *filename);

/**
 * Destroy a JSE context.
 *
 * @param jse_ctx the jse context.
 */
void jse_context_destroy(jse_context_t *jse_ctx);

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
size_t jse_read_file(const char *filename, char **pbuffer, size_t *psize);

#if defined(__cplusplus)
}
#endif

#endif
