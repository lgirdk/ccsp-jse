
#include <duktape.h>
#include "jse_debug.h"
#include "jse_jserror.h"

/**
 * The PosixError.toString() function binding.
 *
 * Returns a string representation of the error. Takes no arguments.
 *
 * @param ctx the duktape context
 * @return an error status or 0.
 */
static duk_ret_t do_posix_error_to_string(duk_context * ctx)
{
    /* Get the this pointer. */
    duk_push_this(ctx);
    /* [ .... this ] */

    /* duktape indices are either 0, 1, 2, 3 from the top or
        -1, -2, -3 from the bottom */
    duk_get_prop_string(ctx, -1, "error");
    /* [ .... this, this.error ] */

    /* duk_push_literal too new for open embedded 3.1 */
    duk_push_string(ctx, ": ");
    /* [ .... this, this.error, ": " ] */

    duk_get_prop_string(ctx, -3, "message");
    /* [ .... this, this.error, ": ", this.message ] */

    duk_push_string(ctx, " (errno=");
    /* [ .... this, this.error, ": ", this.message, " (errno=" ] */

    duk_get_prop_string(ctx, -5, "errno");

    duk_push_string(ctx, ")");
    /* [ .... this, this.error, ": ", this.message, " (errno=", this.errno, ")" ] */

    /* String concat the bottom 6 items on the stack */
    duk_concat(ctx, 6);
    /* [ .... this, this.error + ": " + this.message + " (errno=" + this.errno + ")"" ] */

    duk_swap(ctx, -1, -2);
    /* [ .... this.error ..., this ] */

    duk_pop(ctx);
    /* [ .... this.error ... ] */

    /* One item returned */
    return 1;
}

/**
 * The PosixError.getErrno() function binding.
 *
 * Returns errno. Takes no arguments.
 *
 * @param ctx the duktape context
 * @return an error status or 0.
 */
static duk_ret_t do_posix_error_get_errno(duk_context * ctx)
{
    /* Get this.errno */
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "errno");

    /* Pop this */
    duk_swap(ctx, -1, -2);
    duk_pop(ctx);

    /* One item returned */
    return 1;
}

/**
 * Pushes a PosixError object on to the stack.
 *
 * @param ctx the duktape context.
 * @param _errno the POSIX errno.
 * @param format the message format string.
 * @param ap the parameter list.
 *
 * @return an error status or 0.
 */
static duk_int_t push_posix_error_va(duk_context * ctx, int _errno, const char * format, va_list ap)
{
    /* Create an error object. The only way we can get to an error. */
    duk_push_error_object_va(ctx, DUK_ERR_ERROR, format, ap);
    /* [ .... Error ] */

    duk_push_string(ctx, "PosixError");
    /* [ .... Error, "PosixError" ] */

    duk_put_prop_string(ctx, -2, "error"); /* Error */
    /* [ .... Error ] */

    duk_push_int(ctx, _errno);
    /* [ .... Error, _errno ] */

    /* This creates the errno field. */
    duk_put_prop_string(ctx, -2, "errno"); /* Error */
    /* [ .... Error ] */

    duk_push_c_function(ctx, do_posix_error_to_string, 0);
    /* [ .... Error, do_posix_error_to_string ] */

    duk_put_prop_string(ctx, -2, "toString"); /* Error */
    /* [ .... Error ] */

    duk_push_c_function(ctx, do_posix_error_get_errno, 0);
    /* [ .... Error, do_posix_error_get_errno ] */

    duk_put_prop_string(ctx, -2, "getErrno"); /* Error */
    /* [ .... Error ] */

    return 0;
}

/**
 * Pushes a PosixError object on to the stack. Varargs version.
 *
 * @param ctx the duktape context.
 * @param _errno the POSIX errno.
 * @param format the message format string followed by parameters.
 *
 * @return an error status or 0.
 */
static duk_int_t push_posix_error(duk_context * ctx, int _errno, const char * format, ...)
{
    duk_int_t ret = DUK_ERR_ERROR;
    va_list ap;

    va_start(ap, format);
    ret = push_posix_error_va(ctx, _errno, format, ap);
    va_end(ap);

    return ret;
}

/**
 * Throws a PosixError object.
 *
 * @param ctx the duktape context.
 * @param _errno the POSIX errno.
 * @param format the format string followed by arguments.
 */
void jse_throw_posix_error(duk_context * ctx, int _errno, const char * format, ...)
{
    duk_int_t ret = DUK_ERR_ERROR;
    va_list ap;

    va_start(ap, format);
    ret = push_posix_error_va(ctx, _errno, format, ap);
    va_end(ap);

    if (ret == 0)
    {
        /* [ PosixError ] */

        /* Never returns */
        (void) duk_throw(ctx);
    }
    else
    {
        /* Never returns */
        JSE_THROW_ERROR(ctx, DUK_ERR_ERROR, "Failed to construct PosixError object!");
    }
}

/**
 * The PosixError constructor function binding.
 *
 * Constructs a new PosixError object. The constructor takes two
 * arguments, an integer that is the errno value, and a message
 * string.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t do_new_posix_error(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    if (!duk_is_constructor_call(ctx))
    {
        /* Never returns */
        JSE_THROW_TYPE_ERROR(ctx, "Not called as a constructor!");
    }
    else
    {
        duk_idx_t num = duk_get_top(ctx);

        if (num != 2)
        {
            /* Never returns */
            JSE_THROW_TYPE_ERROR(ctx, "Invalid number of arguments!");
        }
        else
        {
            if (duk_get_type(ctx, -2) != DUK_TYPE_NUMBER || duk_get_type(ctx, -1) != DUK_TYPE_STRING)
            {
                /* Never returns */
                JSE_THROW_TYPE_ERROR(ctx, "Invalid argument type!");
            }
            else
            {
                int _errno = duk_get_int_default(ctx, 0, 0);
                const char* message = duk_safe_to_string(ctx, 1);

                if (push_posix_error(ctx, _errno, "%s", message) == 0)
                {
                    ret = 1;
                }
            }
        }
    }

    return ret;
}

/**
 * The throwPosixError constructor function binding.
 *
 * Constructs a new PosixError object and throws it. It takes two
 * arguments, an integer that is the errno value, and a message
 * string.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t do_throw_posix_error(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    duk_idx_t num = duk_get_top(ctx);

    if (num != 2)
    {
        /* Never returns */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid number of arguments!");
    }
    else
    {
        if (duk_get_type(ctx, -2) != DUK_TYPE_NUMBER || duk_get_type(ctx, -1) != DUK_TYPE_STRING)
        {
            /* Never returns */
            JSE_THROW_TYPE_ERROR(ctx, "Invalid argument type!");
        }
        else
        {
            int _errno = duk_get_int_default(ctx, 0, 0);
            const char* message = duk_safe_to_string(ctx, 1);

            if (push_posix_error(ctx, _errno, "%s", message) == 0)
            {
                /* Never returns */
                (void) duk_throw(ctx);
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
duk_int_t jse_bind_jserror(jse_context_t * jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_VERBOSE("Binding JS errors!")

    if (jse_ctx != NULL)
    {
        duk_push_c_function(jse_ctx->ctx, do_new_posix_error, 2);
        duk_put_global_string(jse_ctx->ctx, "PosixError");

        duk_push_c_function(jse_ctx->ctx, do_throw_posix_error, 2);
        duk_put_global_string(jse_ctx->ctx, "throwPosixError");

        ret = 0;
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}
