
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include "jse_debug.h"
#include "jse_jscommon.h"
#include "jse_jserror.h"

/** Reference count for binding. */
static int ref_count = 0;

/**
 * Runs the code stored in a buffer, optionally identified by a filename.
 * 
 * @param ctx the duktape content.
 * @param buffer the buffer.
 * @param size the size of the context.
 * @param filename the optional filename (may be null).
 * @return an error status or 0.
 */
static duk_int_t run_buffer(duk_context * ctx, const char * buffer, size_t size, const char * filename)
{
    duk_int_t ret = DUK_EXEC_ERROR;

    JSE_ASSERT(buffer != NULL)
    JSE_ASSERT(size != 0)

    if (filename != NULL)
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
    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Runs JavaScript code stored in a buffer.
 *
 * @param jse_ctx the jse context.
 * @param buffer the buffer.
 * @param size the size of the context.
 * @return an error status or 0.
 */
duk_int_t jse_run_buffer(jse_context_t * jse_ctx, const char * buffer, size_t size)
{
    duk_int_t ret = DUK_EXEC_ERROR;

    if (jse_ctx != NULL && buffer != NULL && size != 0)
    {
        ret = run_buffer(jse_ctx->ctx, buffer, size, jse_ctx->filename);
    }
    else
    {
        JSE_ERROR("Invalid arguments!")
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Runs JavaScript code stored in a file, the name of which is on the stack.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t do_include(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    const char * filename = NULL;
    char * buffer = NULL;
    size_t size = 0;
    ssize_t bytes = 0;

    JSE_ASSERT(ctx != NULL)

    JSE_VERBOSE("do_include()")

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

                    if (run_buffer(ctx, buffer, size, filename) == DUK_EXEC_SUCCESS)
                    {
                        ret = 0;
                    }

                    free(buffer);
                }
            }
        }
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * A JavaScript debug print binding.
 *
 * This binds a JavaScript function that uses the JSE debug_print() method
 * to print to the debug log. It takes the following arguments:
 *  - debug message
 *  - debug level (optional)
 *  - file name (optional)
 *  - line number (optional)
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t do_debugPrint(duk_context * ctx)
{
#ifdef JSE_DEBUG_ENABLED
    duk_int_t count = duk_get_top(ctx);
    char * filename = "SCRIPT";
    char * msg = NULL;
    int level = JSE_DEBUG_LEVEL_DEBUG;
    int line = 0;

    JSE_VERBOSE("do_debugPrint()")

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
            filename = (char*)duk_safe_to_string(ctx, 2);
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
        char tmp[16];

        snprintf(tmp, sizeof(tmp), "LEVEL:%d", level);
        jse_debugPrint(filename, line, tmp, "%s", msg);
    }
#endif

    return 0;
}

/**
 * The binding for readFileAsString()
 *
 * This function reads the file specified in the first argument in to
 * a buffer and returns that buffer as a JavaScript string.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t do_read_file_as_string(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    const char * filename = NULL;
    char * buffer = NULL;
    size_t size = 0;
    ssize_t bytes = 0;

    JSE_VERBOSE("do_read_file_as_string()")

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

                    duk_push_lstring(ctx, buffer, (duk_size_t)size);
                    free(buffer);

                    ret = 1;
                }
            }
        }
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * The binding for writeAsFile()
 *
 * This function writes in to the file specified in the first argument the
 * value passed as the second argument. The third optional argument is a
 * boolean which, if true, will create the file if it does not exist.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t do_write_as_file(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    duk_int_t count = duk_get_top(ctx);
    const char * filename = NULL;

    JSE_VERBOSE("do_write_as_file()")

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
                /* TODO: Handle other data types eg buffers */
                duk_int_t type = duk_get_type(ctx, 1);
                if (DUK_TYPE_BOOLEAN == type || DUK_TYPE_NUMBER == type ||
                    DUK_TYPE_OBJECT == type || DUK_TYPE_STRING == type)
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

                    JSE_VERBOSE("Opening %s %d", filename, flags)
                    fd = open(filename, flags, S_IRWXU);
                    if (fd == -1)
                    {
                        /* This does not return */
                        JSE_THROW_POSIX_ERROR(ctx, errno, "%s: %s", filename, strerror(errno));
                    }
                    else
                    {
                        const char * str = duk_safe_to_string(ctx, 1);
                        size_t len = strlen(str);
                        ssize_t bytes = -1;

                        TEMP_FAILURE_RETRY(bytes = write(fd, str, len));

                        if (bytes == -1) {
                            close(fd);
                            /* This does not return */
                            JSE_THROW_POSIX_ERROR(ctx, errno, "%s: %s", filename, strerror(errno));
                        }

                        close(fd);
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

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * The binding for removeFile()
 *
 * This function removes the file specified in the string argument.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t do_remove_file(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    const char * filename = NULL;

    JSE_VERBOSE("do_read_file_as_string()")

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

    return ret;
}

/**
 * The binding for listDirectory()
 *
 * This function lists the directory specified in the string argument.
 * This function returns all items in the directory, files, 
 * sub-durectories, symlinks etc. It does not recurse.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t do_list_directory(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    const char * dirname = NULL;

    JSE_VERBOSE("do_list_directory()")

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

    return ret;
}

/**
 * The binding for sleep()
 *
 * This function sleeps the process for the specified number of seconds.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
duk_ret_t do_sleep(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    if (duk_is_number(ctx, -1))
    {
        unsigned int delay = (unsigned int)duk_get_uint_default(ctx, -1, 0);
        do
        {
            delay = sleep(delay);
        } 
        while (delay > 0);
        
        ret = 0;
    }
    else
    {
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument!");
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * The binding for usleep()
 *
 * This function sleeps the process for the specified number of microseconds.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
duk_ret_t do_usleep(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    if (duk_is_number(ctx, -1))
    {
        unsigned int delay = (unsigned int)duk_get_uint_default(ctx, -1, 0);
        struct timespec req;
        struct timespec rem;
        int err = -1;

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

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Binds a set of JavaScript extensions
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_jscommon(jse_context_t * jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_VERBOSE("Binding JS common!")

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

                duk_push_c_function(jse_ctx->ctx, do_write_as_file, DUK_VARARGS);
                duk_put_global_string(jse_ctx->ctx, "writeAsFile");

                duk_push_c_function(jse_ctx->ctx, do_remove_file, 1);
                duk_put_global_string(jse_ctx->ctx, "removeFile");

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

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Unbinds the JavaScript extensions.
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
    ref_count --;
    JSE_VERBOSE("ref_count=%d", ref_count)

    /* TODO: Actually unbind */

    jse_unbind_jserror(jse_ctx);
}
