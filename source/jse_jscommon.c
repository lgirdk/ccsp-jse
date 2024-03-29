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

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>

#include "jse_debug.h"
#include "jse_jscommon.h"
#include "jse_jserror.h"

/** Reference count for binding. */
static int ref_count = 0;

/**
 * @brief Runs the code stored in a buffer, optionally identified by a filename.
 *
 * @param ctx the duktape content.
 * @param buffer the buffer.
 * @param size the size of the context.
 * @param filename the optional filename (may be null).
 *
 * @return an error status or 0.
 */
static duk_int_t run_buffer(duk_context * ctx, const char * buffer, size_t size, const char * filename)
{
    duk_int_t ret = DUK_EXEC_ERROR;

    JSE_ASSERT(buffer != NULL)
    JSE_ASSERT(size != 0)

    JSE_ENTER("run_buffer(%p,%p,%u,\"%s\")", ctx, buffer, size, filename)

    // Check if buffer starts with the bytecode marker byte which never occurs in a valid extended UTF-8 string.
    if (buffer[0] == (char)0xbf)
    {
        void *bc = duk_push_fixed_buffer(ctx, size);
        memcpy(bc, (const void *)buffer, size);
        duk_load_function(ctx);
        ret = 0;
    }
    else if (filename != NULL)
    {
        duk_push_string(ctx, filename);
        ret = duk_pcompile_lstring_filename(ctx, DUK_COMPILE_SHEBANG, buffer, size);
    }
    else
    {
        ret = duk_pcompile_lstring(ctx, DUK_COMPILE_SHEBANG, buffer, size);
    }

    if (ret != 0)
    {
        JSE_ERROR("Compile failed!")
    }
    else
    {
        ret = duk_pcall(ctx, 0);
        if (ret != DUK_EXEC_SUCCESS)
        {
            JSE_ERROR("Execution failed!")
        }
        else
        {
            /* duk_safe_to_string() coerces the value on the stack in to a string
               leaving it on the stack (as well as returning the value) */
            JSE_DEBUG("Results: %s", duk_safe_to_string(ctx, -1))

            /* pop the coerced string from the stack */
            duk_pop(ctx);
        }
    }

    /* In case of error, an Error() object is left on the stack */
    JSE_EXIT("run_buffer()=%d", ret)
    return ret;
}

/**
 * @brief Runs JavaScript code stored in a buffer.
 *
 * @param jse_ctx the jse context.
 * @param buffer the buffer.
 * @param size the size of the context.
 *
 * @return an error status or 0.
 */
duk_int_t jse_run_buffer(jse_context_t * jse_ctx, const char * buffer, size_t size)
{
    duk_int_t ret = DUK_EXEC_ERROR;

    JSE_ENTER("jse_run_buffer(%p,%p,%u)", jse_ctx, buffer, size)

    if (jse_ctx != NULL && buffer != NULL && size != 0)
    {
        ret = run_buffer(jse_ctx->ctx, buffer, size, jse_ctx->filename);
    }
    else
    {
        JSE_ERROR("Invalid arguments!")
    }

    JSE_EXIT("jse_run_buffer()=%d", ret)
    return ret;
}

/**
 * @brief A JavaScript include binding.
 *
 * This JS function reads the file specified in the first argument, in to
 * a buffer, and executes it. This will add all globals in the executed
 * file to the global context.
 *
 * @param ctx the duktape context.
 * @return 0 or a negative error.
 */
static duk_ret_t do_include(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    const char * filename = NULL;
    void * buffer = NULL;
    size_t size = 0;
    ssize_t bytes = 0;

    JSE_ASSERT(ctx != NULL)

    JSE_ENTER("do_include(%p)", ctx)

    if (!duk_is_string(ctx, -1))
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Filename is not a string!");
    }
    else
    {
        filename = duk_safe_to_string(ctx, -1);
        if (filename == NULL)
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Filename is null");
        }
        else
        {
            struct stat s;

            if (stat(filename, &s) != 0)
            {
                JSE_THROW_POSIX_ERROR(ctx, errno, "%s: %s", filename, strerror(errno));
            }
            else
            if (!S_ISREG(s.st_mode))
            {
                JSE_THROW_URI_ERROR(ctx, "%s: not a regular file", filename);
            }
            else
            {
                bytes = jse_read_file(filename, &buffer, &size);
                if (bytes == -1)
                {
                    JSE_ERROR("Including %s failed", filename)
                }
                else
                {
                    JSE_ASSERT(buffer != NULL)
                    JSE_ASSERT(size != 0)

                    if (run_buffer(ctx, (char*)buffer, size, filename) == DUK_EXEC_SUCCESS)
                    {
                        ret = 0;
                    }

                    free(buffer);
                }
            }
        }
    }

    JSE_EXIT("do_include()=%d", ret)
    return ret;
}

/**
 * @brief A JavaScript debug print binding.
 *
 * This binds a JavaScript function that uses the JSE debug_print() method
 * to print to the debug log. It takes the following arguments:
 *  - debug message
 *  - debug level (optional)
 *  - file name (optional)
 *  - line number (optional)
 *
 * @param ctx the duktape context.
 *
 * @return an error status or 0.
 */
static duk_ret_t do_debugPrint(duk_context * ctx)
{
#ifdef JSE_DEBUG_ENABLED
    duk_int_t count = duk_get_top(ctx);
    const char * filename = "SCRIPT";
    char * msg = NULL;
    int level = JSE_DEBUG_LEVEL_DEBUG;
    int line = 0;

    JSE_ENTER("do_debugPrint(%p)", ctx)

    /* Must have at least one argument */
    if (count == 0)
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Insufficient arguments!");
    }

    /* debug level */
    if (count > 1)
    {
        /* duktape indices are either 0, 1, 2, 3 from the top or
           -1, -2, -3 from the bottom */
        if (!duk_is_number(ctx, 1))
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Level is not a number!");
        }
        else
        {
            level = (int)duk_get_int_default(ctx, 1, JSE_DEBUG_LEVEL_DEBUG);
        }
    }

    /* filename */
    if (count > 2)
    {
        if (!duk_is_string(ctx, 2))
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Filename is not a string!");
        }
        else
        {
            filename = duk_safe_to_string(ctx, 2);
        }
    }

    /* line number */
    if (count > 3)
    {
        if (!duk_is_number(ctx, 3))
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Line is not a number!");
        }
        else
        {
            line = (int)duk_get_int_default(ctx, 3, 0);
        }
    }

    msg = (char*)duk_safe_to_string(ctx, 0);

    if (level <= JSE_DEBUG_LEVEL_MAX)
    {
        jse_debugPrint(filename, line, jse_debugGetLevel(level), "%s", msg);
    }
    else
    {
        char tmp[20];

        snprintf(tmp, sizeof(tmp), "LEVEL:%d", level);
        jse_debugPrint(filename, line, tmp, "%s", msg);
    }
#endif

    /* Returns no items on the value stack */
    JSE_EXIT("debugPrint()=0")
    return 0;
}

/**
 * @brief A JavaScript binding for readFileAsString()
 *
 * This JS function reads the file specified in the first argument, in to
 * a buffer, and returns that buffer as a JavaScript string.
 *
 * @param ctx the duktape context.
 *
 * @return 1 or a negative error status.
 */
static duk_ret_t do_read_file_as_string(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    const char * filename = NULL;
    void * buffer = NULL;
    size_t size = 0;
    ssize_t bytes = 0;

    JSE_ENTER("do_read_file_as_string(%p)", ctx)

    if (!duk_is_string(ctx, -1))
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Filename is not a string!");
    }
    else
    {
        filename = duk_safe_to_string(ctx, -1);
        if (filename == NULL)
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Filename is null");
        }
        else
        {
            struct stat s;

            if (stat(filename, &s) != 0)
            {
                /* This does not return */
                JSE_THROW_POSIX_ERROR(ctx, errno, "%s: %s", filename, strerror(errno));
            }
            else
            if (!S_ISREG(s.st_mode))
            {
                /* This does not return */
                JSE_THROW_URI_ERROR(ctx, "%s: not a regular file", filename);
            }
            else
            {
                bytes = jse_read_file(filename, &buffer, &size);
                if (bytes == -1)
                {
                    JSE_ERROR("Failed to read: %s", filename)
                }
                else
                {
                    JSE_ASSERT(buffer != NULL)
                    JSE_ASSERT(size != 0)

                    JSE_VERBOSE("buffer=%p, size=%d", buffer, size)

                    duk_push_lstring(ctx, (const char*)buffer, (duk_size_t)size);
                    free(buffer);

                    /* Returns a single string on the value stack */
                    ret = 1;
                }
            }
        }
    }

    JSE_EXIT("do_read_file_as_string()=%d", ret)
    return ret;
}

/**
 * @brief The JavaScript binding for readFileAsBuffer()
 *
 * This JS function reads the file specified in the first argument in to
 * a buffer and returns that buffer as a JavaScript buffer object.
 * This function should be used when the type of the data is known not
 * to be a string or is unknown.
 *
 * @param ctx the duktape context.
 *
 * @return 1 or a negative error status.
 */
static duk_ret_t do_read_file_as_buffer(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    const char * filename = NULL;
    void * buffer = NULL;
    size_t size = 0;
    ssize_t bytes = 0;

    JSE_ENTER("do_read_file_as_buffer(%p)", ctx)

    if (!duk_is_string(ctx, -1))
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Filename is not a string!");
    }
    else
    {
        filename = duk_safe_to_string(ctx, -1);
        if (filename == NULL)
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Filename is null");
        }
        else
        {
            struct stat s;

            if (stat(filename, &s) != 0)
            {
                /* This does not return */
                JSE_THROW_POSIX_ERROR(ctx, errno, "%s: %s", filename, strerror(errno));
            }
            else
            if (!S_ISREG(s.st_mode))
            {
                /* This does not return */
                JSE_THROW_URI_ERROR(ctx, "%s: not a regular file", filename);
            }
            else
            {
                bytes = jse_read_file(filename, &buffer, &size);
                if (bytes == -1)
                {
                    JSE_ERROR("Failed to read: %s", filename)
                }
                else
                {
                    void * dukbuf = NULL;

                    JSE_ASSERT(buffer != NULL)
                    JSE_ASSERT(size != 0)

                    JSE_VERBOSE("buffer=%p, size=%d", buffer, size)

                    dukbuf = duk_push_fixed_buffer(ctx, size);
                    memcpy(dukbuf, buffer, size);
                    free(buffer);

                    /* Returns a single buffer on the value stack */
                    ret = 1;
                }
            }
        }
    }

    JSE_EXIT("do_read_file_as_buffer()=%d", ret)
    return ret;
}

/**
 * @brief The JavaScript binding for writeAsFile()
 *
 * This JS function writes in to the file specified in the first argument the
 * value passed as the second argument. The third optional argument is a
 * boolean which, if true, will create the file if it does not exist.
 *
 * @param ctx the duktape context.
 *
 * @return 0 or a negative error status.
 */
static duk_ret_t do_write_as_file(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    duk_int_t count = duk_get_top(ctx);
    const char * filename = NULL;
    pid_t pid = getpid();

    JSE_ENTER("do_write_as_file(%p)", ctx)

    if (count < 2)
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Insufficient arguments!");
    }
    else
    {
        if (!duk_is_string(ctx, 0))
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Filename is not a string!");
        }
        else
        {
            filename = duk_safe_to_string(ctx, 0);
            if (filename == NULL)
            {
                /* This does not return */
                JSE_THROW_TYPE_ERROR(ctx, "Filename is null");
            }
            else
            {
                char * fn = strdup(filename);
                char * path = dirname(fn);
                char * tmp_filename = malloc(PATH_MAX);
                snprintf(tmp_filename, PATH_MAX, "%s/tmp_%ld", path, (long) pid);
                free(fn);

                /* TODO: Handle other data types eg buffers */
                duk_int_t type = duk_get_type(ctx, 1);
                if (DUK_TYPE_BOOLEAN == type || DUK_TYPE_NUMBER == type ||
                    DUK_TYPE_OBJECT == type || DUK_TYPE_STRING == type ||
                    DUK_TYPE_BUFFER == type)
                {
                    int flags = O_WRONLY | O_TRUNC;
                    int fd = -1;

                    if (count > 2)
                    {
                        if (duk_get_boolean(ctx, 2))
                        {
                            flags |= O_CREAT;
                        }
                    }

                    JSE_VERBOSE("Opening %s %d", tmp_filename, flags)
                    fd = open(tmp_filename, flags, S_IRWXU);
                    if (fd == -1)
                    {
                        free(tmp_filename);
                        /* This does not return */
                        JSE_THROW_POSIX_ERROR(ctx, errno, "%s", strerror(errno));
                    }
                    else
                    {
                        const char * str = NULL;
                        size_t len = 0;
                        size_t off = 0;
                        ssize_t bytes = -1;

                        if (duk_is_buffer_data(ctx, 1))
                        {
                             str = (const char *)duk_get_buffer_data(ctx, 1, &len);
                        }
                        else
                        {
                            str = duk_safe_to_string(ctx, 1);
                            len = strlen(str);
                        }

                        do
                        {
                            TEMP_FAILURE_RETRY(bytes = write(fd, str + off, len));
                            if (bytes > 0)
                            {
                                off += bytes;
                                len -= bytes;
                            }
                        }
                        while((bytes > 0) && (len > 0));

                        if (bytes == -1) {
                            close(fd);
                            free(tmp_filename);
                            /* This does not return */
                            JSE_THROW_POSIX_ERROR(ctx, errno, "%s", strerror(errno));
                        }

                        close(fd);

                        int rn = -1;
                        rn = rename(tmp_filename, filename);
                        if (rn == -1) {
                            free(tmp_filename);
                            /* This does not return */
                            JSE_THROW_POSIX_ERROR(ctx, errno, "%s", strerror(errno));
                        }
                        free(tmp_filename);

                        ret = 0;
                    }
                }
                else
                {
                    /* This does not return */
                    JSE_THROW_TYPE_ERROR(ctx, "Unsupported type: %d", type);
                }
            }
        }
    }

    JSE_EXIT("do_write_as_file()=%d", ret)
    return ret;
}

/**
 * @brief The JavaScript binding for removeFile()
 *
 * This JS function removes the file specified in the string argument.
 *
 * @param ctx the duktape context.
 *
 * @return 0 or a negative error status.
 */
static duk_ret_t do_remove_file(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    const char * filename = NULL;

    JSE_ENTER("do_remove_file(%p)", ctx)

    if (!duk_is_string(ctx, -1))
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Filename is not a string!");
    }
    else
    {
        filename = duk_safe_to_string(ctx, 0);
        if (filename == NULL)
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Filename is null");
        }
        else
        {
            if (unlink(filename) == -1)
            {
                /* This does not return */
                JSE_THROW_POSIX_ERROR(ctx, errno, "%s: %s", filename, strerror(errno));
            }

            ret = 0;
        }
    }

    JSE_EXIT("do_remove_file()=%d", ret)
    return ret;
}

/**
 * @brief The JavaScript binding for createDirectory()
 *
 * This JS function creates the directory specified in the string argument.
 * This function will create any subdirectories required in the path.
 * Existing directories are not an error.
 *
 * @param ctx the duktape context.
 *
 * @return 0 or a negative error status.
 */
static duk_ret_t do_create_directory(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    JSE_ENTER("do_create_directory(%p)", ctx)

    if (!duk_is_string(ctx, -1))
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Directory name is not a string!");
    }
    else
    {
        const char * dirname = duk_safe_to_string(ctx, 0);
        int mderr = jse_mkdir(dirname);

        if (mderr != 0)
        {
            JSE_THROW_POSIX_ERROR(ctx, mderr, "Failed to create directory: %s", strerror(mderr));
        }
        else
        {
            ret = 0;
        }
    }

    return ret;
}

/**
 * @brief The JavaScript binding for listDirectory()
 *
 * This JS function lists the directory specified in the string argument.
 * This function returns all items in the directory, files,
 * sub-durectories, symlinks etc. It does not recurse.
 *
 * @param ctx the duktape context.
 *
 * @return 1 or a negative error status.
 */
static duk_ret_t do_list_directory(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    const char * dirname = NULL;

    JSE_ENTER("do_list_directory(%p)", ctx)

    if (!duk_is_string(ctx, -1))
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Directory name is not a string!");
    }
    else
    {
        dirname = duk_safe_to_string(ctx, 0);
        if (dirname == NULL)
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Dirname is null");
        }
        else
        {
            struct stat s;

            JSE_VERBOSE("dirname=%s", dirname)

            if (stat(dirname, &s) != 0)
            {
                /* This does not return */
                JSE_THROW_POSIX_ERROR(ctx, errno, "%s: %s", dirname, strerror(errno));
            }
            else
            if (!S_ISDIR(s.st_mode))
            {
                /* This does not return */
                JSE_THROW_URI_ERROR(ctx, "%s: not a directory", dirname);
            }
            else
            {
                DIR* dir = opendir(dirname);
                if (dir == NULL)
                {
                    /* This does not return */
                    JSE_THROW_POSIX_ERROR(ctx, errno, "%s: %s", dirname, strerror(errno));
                }
                else
                {
                    duk_idx_t arr_idx = duk_push_array(ctx);
                    struct dirent * de = NULL;
                    duk_idx_t idx = 0;

                    /* [ .... Array ] */

                    for (idx = 0; /* EVER */ ; idx++)
                    {
                        errno = 0; /* Needed to test for errors */
                        de = readdir(dir);
                        if (de == NULL)
                        {
                            /* NULL is end of list OR an error indicated by errno */
                            if (errno != 0)
                            {
                                (void) closedir(dir);
                                /* This does not return */
                                JSE_THROW_POSIX_ERROR(ctx, errno, "%s: %s", dirname, strerror(errno));
                            }

                            break;
                        }

                        duk_push_string(ctx, de->d_name);
                        /* [ .... Array, d_name ] */

                        duk_put_prop_index(ctx, arr_idx, idx);
                        /* [ .... Array ] */
                    }

                    (void) closedir(dir);

                    /* Return 1 item, the array */
                    ret = 1;
                }
            }
        }
    }

    JSE_EXIT("do_list_directory()=%d", ret)
    return ret;
}

/**
 * @brief The JavaScript binding for sleep()
 *
 * This JS function sleeps the process for the specified number of seconds.
 *
 * @param ctx the duktape context.
 *
 * @return 0 or a negative error status.
 */
duk_ret_t do_sleep(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    JSE_ENTER("do_sleep(%p)", ctx)

    if (duk_is_number(ctx, -1))
    {
        unsigned int delay = (unsigned int)duk_get_uint_default(ctx, -1, 0);

        JSE_VERBOSE("delay=%u", delay)

        do
        {
            delay = sleep(delay);
        }
        while (delay > 0);

        /* Nothing is returned on the value stack */
        ret = 0;
    }
    else
    {
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument!");
    }

    JSE_EXIT("do_sleep()=%d", ret)
    return ret;
}

/**
 * @brief The binding for usleep()
 *
 * This function sleeps the process for the specified number of microseconds.
 *
 * @param ctx the duktape context.
 *
 * @return 0 or a negative error status.
 */
duk_ret_t do_usleep(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    JSE_ENTER("do_usleep(%p)", ctx)

    if (duk_is_number(ctx, -1))
    {
        unsigned int delay = (unsigned int)duk_get_uint_default(ctx, -1, 0);
        struct timespec req;
        struct timespec rem;
        int err = -1;

        JSE_VERBOSE("delay=%u", delay)

        req.tv_sec  = (delay / 1000000);
        req.tv_nsec = (delay % 1000000) * 1000;

        do
        {
            err = nanosleep(&req, &rem);
            req.tv_sec  = rem.tv_sec;
            req.tv_nsec = rem.tv_nsec;
        }
        while ((err == -1) && (errno == EINTR));

        if (err == -1)
        {
            /* Does not return */
            JSE_THROW_POSIX_ERROR(ctx, errno, "nanosleep() failed: %s", strerror(errno));
        }

        ret = 0;
    }
    else
    {
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument!");
    }

    JSE_EXIT("do_usleep()=%d", ret)
    return ret;
}

/**
 * @brief Binds a set of JavaScript extensions
 *
 * @param jse_ctx the jse context.
 *
 * @return an error status or 0.
 */
duk_int_t jse_bind_jscommon(jse_context_t * jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_ENTER("jse_bind_jscommon(%p)", jse_ctx)

    JSE_VERBOSE("ref_count=%d", ref_count)
    if (jse_ctx != NULL)
    {
        /* jscommon is dependent upon jserror error objects so bind here */
        if ((ret = jse_bind_jserror(jse_ctx)) != 0)
        {
            JSE_ERROR("Failed to bind JS error objects")
        }
        else
        {
            if (ref_count == 0)
            {
                duk_push_c_function(jse_ctx->ctx, do_include, 1);
                duk_put_global_string(jse_ctx->ctx, "include");

                duk_push_c_function(jse_ctx->ctx, do_debugPrint, DUK_VARARGS);
                duk_put_global_string(jse_ctx->ctx, "debugPrint");

                duk_push_c_function(jse_ctx->ctx, do_read_file_as_string, 1);
                duk_put_global_string(jse_ctx->ctx, "readFileAsString");

                duk_push_c_function(jse_ctx->ctx, do_read_file_as_buffer, 1);
                duk_put_global_string(jse_ctx->ctx, "readFileAsBuffer");

                duk_push_c_function(jse_ctx->ctx, do_write_as_file, DUK_VARARGS);
                duk_put_global_string(jse_ctx->ctx, "writeAsFile");

                duk_push_c_function(jse_ctx->ctx, do_remove_file, 1);
                duk_put_global_string(jse_ctx->ctx, "removeFile");

                duk_push_c_function(jse_ctx->ctx, do_create_directory, 1);
                duk_put_global_string(jse_ctx->ctx, "createDirectory");

                duk_push_c_function(jse_ctx->ctx, do_list_directory, 1);
                duk_put_global_string(jse_ctx->ctx, "listDirectory");

                duk_push_c_function(jse_ctx->ctx, do_sleep, 1);
                duk_put_global_string(jse_ctx->ctx, "sleep");

                duk_push_c_function(jse_ctx->ctx, do_usleep, 1);
                duk_put_global_string(jse_ctx->ctx, "usleep");
             }

            ref_count ++;
            ret = 0;
        }
    }

    JSE_EXIT("jse_bind_jscommon()=%d", ret)
    return ret;
}

/**
 * @brief Unbinds the JavaScript extensions.
 *
 * Actually just decrements the reference count. Needed for fast cgi
 * since the same process will rebind. Not unbinding is not an issue
 * as the duktape context is destroyed each time cleaning everything
 * up.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_jscommon(jse_context_t * jse_ctx)
{
    JSE_ENTER("jse_unbind_jscommon(%p)", jse_ctx)

    ref_count --;
    JSE_VERBOSE("ref_count=%d", ref_count)

    /* TODO: Actually unbind */

    jse_unbind_jserror(jse_ctx);

    JSE_EXIT("jse_unbind_jscommon()")
}
