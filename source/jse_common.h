/*****************************************************************************
*
* Copyright 2020 Liberty Global B.V.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*****************************************************************************/

#ifndef JSE_COMMON_H
#define JSE_COMMON_H

#include <stdlib.h>
#include <duktape.h>
#include <qdecoder.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef TEMP_FAILURE_RETRY
/** A EINTR retry macro */
#define TEMP_FAILURE_RETRY(exp) \
    do {} while (((long)(exp)) == -1 && errno == EINTR)
#endif

#define JSE_MAX_FILE_SIZE (128 * 1024)

/** Context for processing the request. Used by the fatal error handler */
struct jse_context_s {
    /** The filename */
    char * filename;
    /** The Duktape content */
    duk_context * ctx;
    /** The head of the request data */
    qentry_t * req;
};

/** The JSE context type */
typedef struct jse_context_s jse_context_t;

/**
 * @brief Create a new JSE context.
 *
 * @param filename the script filename.
 * @return the context or NULL on error.
 */
jse_context_t *jse_context_create(char * filename);

/**
 * @brief Destroy a JSE context.
 *
 * @param jse_ctx the jse context.
 */
void jse_context_destroy(jse_context_t *jse_ctx);

/**
 * @brief A single read from a file descriptor in to a buffer resizing if needed.
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
ssize_t jse_read_fd_once(int fd, void ** const pbuffer, off_t * const poff, size_t * const psize);

/**
 * @brief Read from a file descriptor.
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
ssize_t jse_read_fd(int fd, void ** const pbuffer, size_t * const psize);

/**
 * @brief Reads a file in to a buffer.
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
ssize_t jse_read_file(const char * const filename, void ** const pbuffer, size_t * const psize);

/**
 * @brief Creates a sub directory and all the intermediate directories.
 *
 * Creates a sub directory and all the intermediate directories. If the
 * directory currently exists that is not an error. If a file exists with
 * the same name, or any elements in the path are not a directory, that
 * is an error. Errors also occur due to permissions, read only file
 * systems and so on.
 *
 * @param path the sub directory path.
 *
 * @return 0 on success or an errno.
 */
int jse_mkdir(const char* path);

#if defined(__cplusplus)
}
#endif

#endif
