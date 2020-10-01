
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

#include <duktape.h>
#include "jse_debug.h"
#include "jse_jserror.h"
#include "jse_cosa_error.h"

/** Reference count for binding. */
static int ref_count = 0;

/**
 * The CosaError.toString() function binding.
 *
 * Returns a string representation of the error. Takes no arguments.
 *
 * @param ctx the duktape context
 * @return an error status or 0.
 */
static duk_ret_t do_cosa_error_to_string(duk_context * ctx)
{
    /* Get the this pointer. */
    duk_push_this(ctx);
    /* [ .... this ] */

    /* duktape indices are either 0, 1, 2, 3 from the top or
        -1, -2, -3 from the bottom */
    duk_get_prop_string(ctx, -1, "name");
    /* [ .... this, this.name ] */

    /* duk_push_literal too new for open embedded 3.1 */
    duk_push_string(ctx, ": ");
    /* [ .... this, this.name, ": " ] */

    duk_get_prop_string(ctx, -3, "message");
    /* [ .... this, this.name, ": ", this.message ] */

    duk_push_string(ctx, " (errorCode=");
    /* [ .... this, this.name, ": ", this.message, " (errorCode=" ] */

    duk_get_prop_string(ctx, -5, "errorCode");

    duk_push_string(ctx, ")");
    /* [ .... this, this.name, ": ", this.message, " (errorCode=", this.errorCode, ")" ] */

    /* String concat the bottom 6 items on the stack */
    duk_concat(ctx, 6);
    /* [ .... this, this.name + ": " + this.message + " (errorCode=" + this.errorCode + ")"" ] */

    duk_swap(ctx, -1, -2);
    /* [ .... this.name + ..., this ] */

    duk_pop(ctx);
    /* [ .... this.name + ... ] */

    /* One item returned */
    return 1;
}

/**
 * The CosaError.getErrorCode() function binding.
 *
 * Returns errno. Takes no arguments.
 *
 * @param ctx the duktape context
 * @return an error status or 0.
 */
static duk_ret_t do_cosa_error_get_error_code(duk_context * ctx)
{
    /* Get this.errno */
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "errorCode");

    /* Pop this */
    duk_swap(ctx, -1, -2);
    duk_pop(ctx);

    /* One item returned */
    return 1;
}

/**
 * Pushes a CosaError object on to the stack.
 *
 * @param ctx the duktape context.
 * @param error_code the Cosa error code.
 * @param format the message format string.
 * @param ap the parameter list.
 *
 * @return an error status or 0.
 */
static duk_int_t push_cosa_error_va(duk_context * ctx, int error_code, const char * format, va_list ap)
{
    /* Create an error object. The only way we can get to an error. */
    duk_push_error_object_va(ctx, DUK_ERR_ERROR, format, ap);
    /* [ .... Error ] */

    duk_push_string(ctx, "CosaError");
    /* [ .... Error, "PosixError" ] */

    duk_put_prop_string(ctx, -2, "name"); /* Error */
    /* [ .... Error ] */

    duk_push_int(ctx, error_code);
    /* [ .... Error, error_code ] */

    /* This creates the errorCode field. */
    duk_put_prop_string(ctx, -2, "errorCode"); /* Error */
    /* [ .... Error ] */

    duk_push_c_function(ctx, do_cosa_error_to_string, 0);
    /* [ .... Error, do_posix_error_to_string ] */

    duk_put_prop_string(ctx, -2, "toString"); /* Error */
    /* [ .... Error ] */

    duk_push_c_function(ctx, do_cosa_error_get_error_code, 0);
    /* [ .... Error, do_cosa_error_get_error_code ] */

    duk_put_prop_string(ctx, -2, "getErrorCode"); /* Error */
    /* [ .... Error ] */

    return 0;
}

/**
 * Pushes a PosixError object on to the stack. Varargs version.
 *
 * @param ctx the duktape context.
 * @param error_code the Cosa error code.
 * @param format the message format string followed by parameters.
 *
 * @return an error status or 0.
 */
static duk_int_t push_cosa_error(duk_context * ctx, int error_code, const char * format, ...)
{
    duk_int_t ret = DUK_ERR_ERROR;
    va_list ap;

    va_start(ap, format);
    ret = push_cosa_error_va(ctx, error_code, format, ap);
    va_end(ap);

    return ret;
}

/**
 * Throws a CosaError object.
 *
 * @param ctx the duktape context.
 * @param error_code the Cosa error code.
 * @param format the format string followed by arguments.
 */
void jse_throw_cosa_error(duk_context * ctx, int error_code, const char * format, ...)
{
    duk_int_t ret = DUK_ERR_ERROR;
    va_list ap;

    va_start(ap, format);
    ret = push_cosa_error_va(ctx, error_code, format, ap);
    va_end(ap);

    if (ret == 0)
    {
        /* [ CosaError ] */

        /* Never returns */
        (void) duk_throw(ctx);
    }
    else
    {
        /* Never returns */
        JSE_THROW_ERROR(ctx, DUK_ERR_ERROR, "Failed to construct CosaError object!");
    }
}

/**
 * The PosixError constructor function binding.
 *
 * Constructs a new CosaError object. The constructor takes two
 * arguments, an integer that is the error code, and a message
 * string.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t do_new_cosa_error(duk_context * ctx)
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
                int cosa_error = duk_get_int_default(ctx, 0, 0);
                const char* message = duk_safe_to_string(ctx, 1);

                if (push_cosa_error(ctx, cosa_error, "%s", message) == 0)
                {
                    ret = 1;
                }
            }
        }
    }

    return ret;
}

/**
 * The throwCosaError constructor function binding.
 *
 * Constructs a new CosaError object and throws it. It takes two
 * arguments, an integer that is the error code, and a message
 * string.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t do_throw_cosa_error(duk_context * ctx)
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
            int error_code = duk_get_int_default(ctx, 0, 0);
            const char* message = duk_safe_to_string(ctx, 1);

            if (push_cosa_error(ctx, error_code, "%s", message) == 0)
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
duk_int_t jse_bind_cosa_error(jse_context_t * jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_VERBOSE("Binding Cosa errors!")

    JSE_VERBOSE("ref_count=%d", ref_count)
    if (jse_ctx != NULL)
    {
        if (ref_count == 0)
        {
            duk_push_c_function(jse_ctx->ctx, do_new_cosa_error, 2);
            duk_put_global_string(jse_ctx->ctx, "CosaError");

            duk_push_c_function(jse_ctx->ctx, do_throw_cosa_error, 2);
            duk_put_global_string(jse_ctx->ctx, "throwCosaError");
        }

        ref_count ++;
        ret = 0;
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
void jse_unbind_cosa_error(jse_context_t * jse_ctx)
{
    /* Stop unused warning */
    jse_ctx = jse_ctx;

    ref_count --;
    JSE_VERBOSE("ref_count=%d", ref_count)

    /* TODO: Actually unbind */
}
