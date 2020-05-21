
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "jse_debug.h"
#include "jse_common.h"
#include "jse_jscommon.h"

/**
 * Runs the code stored in a buffer, optionally identified by a filename.
 * 
 * @param ctx the duktape content.
 * @param buffer the buffer.
 * @param size the size of the context.
 * @param filename the optional filename (may be null).
 * @return an error status or 0.
 */
static duk_int_t run_buffer(duk_context *ctx, const char* buffer, size_t size, const char* filename)
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
        JSE_ERROR("Compile failed!");
    }
    else
    {
        ret = duk_pcall(ctx, 0);
        if (ret != DUK_EXEC_SUCCESS)
        {
            JSE_ERROR("Execution failed!");
        }
        else
        {
            /* duk_safe_to_string() coerces the value on the stack in to a string
               leaving it on the stack (as well as returning the value) */
            JSE_DEBUG("Results: %s", duk_safe_to_string(ctx, -1));

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
duk_int_t jse_run_buffer(jse_context_t *jse_ctx, const char* buffer, size_t size)
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
static duk_ret_t do_include(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    const char * filename = NULL;
    char * buffer = NULL;
    size_t size = 0;

    JSE_ASSERT(ctx != NULL)

    JSE_VERBOSE("do_include()")

    if (!duk_is_string(ctx, -1))
    {
        JSE_ERROR("Filename is not a string!")
        (void) duk_type_error(ctx, "Filename is not a string!");
    }
    else
    {
        filename = duk_safe_to_string(ctx, -1);
        if (filename == NULL)
        {
            JSE_ERROR("Filename is null")
        }
        else
        {
            struct stat s;

            if (stat(filename, &s) != 0)
            {
                int errnotmp = errno;
                JSE_ERROR("%s: %s", filename, strerror(errnotmp))
                (void) duk_uri_error(ctx, "%s: %s", filename, strerror(errnotmp));
            }
            else
            if (!S_ISREG(s.st_mode))
            {
                JSE_ERROR("%s: not a regular file", filename)
                (void) duk_uri_error(ctx, "%s: not a regular file", filename);
            }
            else
            {
                size = jse_read_file(filename, &buffer, &size);
                if (size == 0)
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
        (void) duk_type_error(ctx, "Insufficient arguments!");
    }

    /* debug level */
    if (count > 1)
    {
        /* duktape indices are either 0, 1, 2, 3 from the top or
           -1, -2, -3 from the bottom */
        if (!duk_is_number(ctx, 1))
        {
            JSE_ERROR("Level is not a number!");
            (void) duk_type_error(ctx, "Level is not a number!");
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
            JSE_ERROR("Filename is not a string!");
            (void) duk_type_error(ctx, "Filename is not a string!");
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
            JSE_ERROR("Line is not a number!");
            (void) duk_type_error(ctx, "Line is not a number!");
        }
        else
        {
            line = (int)duk_get_int_default(ctx, 3, 0);
        }
    }

    msg = (char*)duk_safe_to_string(ctx, 0);
    jse_debug(filename, line, level, "%s", msg);
    return 0;
#else
    return 0;
#endif
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

    JSE_VERBOSE("do_read_file_as_string()")

    if (!duk_is_string(ctx, -1))
    {
        JSE_ERROR("Filename is not a string!")
        (void) duk_type_error(ctx, "Filename is not a string!");
    }
    else
    {
        filename = duk_safe_to_string(ctx, -1);
        if (filename == NULL)
        {
            JSE_ERROR("Filename is null")
        }
        else
        {
            struct stat s;

            if (stat(filename, &s) != 0)
            {
                int errnotmp = errno;
                JSE_ERROR("%s: %s", filename, strerror(errnotmp))
                (void) duk_uri_error(ctx, "%s: %s", filename, strerror(errnotmp));
            }
            else
            if (!S_ISREG(s.st_mode))
            {
                JSE_ERROR("%s: not a regular file", filename)
                (void) duk_uri_error(ctx, "%s: not a regular file", filename);
            }
            else
            {
                size = jse_read_file(filename, &buffer, &size);
                if (size == 0)
                {
                    JSE_ERROR("Failed to read: %s", filename)
                }
                else
                {
                    JSE_ASSERT(buffer != NULL)
                    JSE_ASSERT(size != 0)

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
        JSE_ERROR("Insufficient arguments!")
        (void) duk_type_error(ctx, "Insufficient arguments!");
    }
    else
    {
        if (!duk_is_string(ctx, 0))
        {
            JSE_ERROR("Filename is not a string!")
            (void) duk_type_error(ctx, "Filename is not a string!");
        }
        else
        {
            filename = duk_safe_to_string(ctx, 0);
            if (filename == NULL)
            {
                JSE_ERROR("Filename is null")
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
                        int errnotmp = errno;
                        JSE_ERROR("%s: %s", filename, strerror(errnotmp))
                        (void) duk_uri_error(ctx, "%s: %s", filename, strerror(errnotmp));
                    }
                    else
                    {
                        const char * str = duk_safe_to_string(ctx, 1);
                        size_t len = strlen(str);
                        ssize_t bytes = -1;

                        TEMP_FAILURE_RETRY(bytes = write(fd, str, len));

                        if (bytes == -1) {
                            int errnotmp = errno;
                            JSE_ERROR("%s: %s", filename, strerror(errnotmp))
                            (void) duk_uri_error(ctx, "%s: %s", filename, strerror(errnotmp));
                        }

                        close(fd);
                        ret = 0;
                    }
                }
                else
                {
                    (void) duk_type_error(ctx, "Unsupported type: %d", type);
                }
            }
        }
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
duk_int_t jse_bind_jscommon(jse_context_t* jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_VERBOSE("Binding JS common!")

    if (jse_ctx != NULL)
    {
        duk_push_c_function(jse_ctx->ctx, do_include, 1);
        duk_put_global_string(jse_ctx->ctx, "include");

        duk_push_c_function(jse_ctx->ctx, do_debugPrint, DUK_VARARGS);
        duk_put_global_string(jse_ctx->ctx, "debugPrint");

        duk_push_c_function(jse_ctx->ctx, do_read_file_as_string, 1);
        duk_put_global_string(jse_ctx->ctx, "readFileAsString");

        duk_push_c_function(jse_ctx->ctx, do_write_as_file, DUK_VARARGS);
        duk_put_global_string(jse_ctx->ctx, "writeAsFile");

        ret = 0;
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}
