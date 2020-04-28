
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
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
    duk_int_t ret = DUK_RET_ERROR;

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
        JSE_ERROR("Compile failed: %s", duk_safe_to_string(ctx, -1));
    }
    else
    {
        ret = duk_pcall(ctx, 0);
        if (ret != DUK_EXEC_SUCCESS)
        {
            JSE_ERROR("Execution failed: %s", duk_safe_to_string(ctx, -1));
        }
        else
        {
            JSE_DEBUG("Results: %s", duk_safe_to_string(ctx, -1));
        }
    } 

    duk_pop(ctx);
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
    duk_int_t ret = DUK_RET_ERROR;

    if (jse_ctx != NULL && buffer != NULL && size != 0)
    {
        ret = run_buffer(jse_ctx->ctx, buffer, size, jse_ctx->filename);
    }
    else
    {
        JSE_ERROR("Invalid arguments!")
    }

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
static duk_int_t do_include(duk_context *ctx)
{
    const char * filename = NULL;
    char * buffer = NULL;
    size_t size = 0;
    duk_int_t ret = DUK_RET_ERROR;

    JSE_ASSERT(ctx != NULL)

    filename = duk_safe_to_string(ctx, -1);
    if (filename == NULL)
    {
        ret = duk_error(ctx, DUK_ERR_TYPE_ERROR, "Filename is null");
    }
    else
    {
        struct stat s;

        if (stat(filename, &s) != 0)
        {
            ret = duk_type_error(ctx, "Filename: %s", strerror(errno));
        }
        else
        if (!S_ISREG(s.st_mode))
        {
            ret = duk_type_error(ctx, "Filename is not a regular file");
        }
        else
        {
            size = jse_read_file(filename, &buffer, &size);
            if (size == 0)
            {
                ret = duk_generic_error(ctx, "Including %s failed", filename);
            }
            else
            {
                JSE_ASSERT(buffer != NULL)
                JSE_ASSERT(size != 0)

                ret = run_buffer(ctx, buffer, size, filename);

                free(buffer);
            }
        }
    }

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
    duk_int_t ret = DUK_RET_ERROR;

    JSE_VERBOSE("Binding JS common!")

    if (jse_ctx != NULL)
    {
        duk_push_c_function(jse_ctx->ctx, do_include, 1);
        duk_put_global_string(jse_ctx->ctx, "include");

        ret = 0;
    }

    return ret;
}


