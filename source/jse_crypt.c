#include <openssl/evp.h>

#include "jse_debug.h"
#include "jse_jserror.h"
#include "jse_crypt.h"

/** Reference count for binding. */
static int ref_count = 0;

/**
 * A JavaScript encryption binding.
 *
 * This binds a JavaScript function that encrypts a value in to a buffer.
 * The function takes four arguments. The first three are required and
 * are the value to encrypt, the key, and the initialisation vector. 
 * The last argument is the encryption type. It defaults to AES 256. The
 * return value is a buffer object containing the encrypted data.
 *
 * @param ctx the duktape context
 * @return an error status or 0.
 */
duk_ret_t do_encrypt(duk_context * ctx)
{
    duk_ret_t ret = -1;
    duk_int_t count = duk_get_top(ctx);

    /* Must have at least three arguments */
    if (count < 3)
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Insufficient arguments!");
    }
    else
    {
        // TODO: Implement real encryption
        const char * dukstr = duk_safe_to_string(ctx, 0);
        size_t duklen = strlen(dukstr);
        void * buffer = duk_push_fixed_buffer(ctx, duklen);

        memcpy(buffer, dukstr, duklen);
        ret = 1;
    }

    return ret;
}

/**
 * A JavaScript decryption binding.
 *
 * This binds a JavaScript function that decrypts a buffer in to a buffer.
 * The function takes four arguments. The first three are required and
 * are the buffer to decrypt, the key, and the initialisation vector. 
 * The last argument is the decryption type. It defaults to AES 256. The
 * return value is a buffer object containing the decrypted data.
 *
 * @param ctx the duktape context
 * @return an error status or 0.
 */
duk_ret_t do_decryptToBuffer(duk_context * ctx)
{
    duk_ret_t ret = -1;
    duk_int_t count = duk_get_top(ctx);

    /* Must have at least three arguments */
    if (count < 3)
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Insufficient arguments!");
    }
    else
    {
        // TODO: Implement real encryption
        if (!duk_is_buffer_data(ctx, 0))
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Argument 1 is not a buffer!");
        }
        else
        {
            // For now return a duplicated buffer.
            duk_dup(ctx, 0);
            ret = 1;
        }
    }

    return ret;
}

/**
 * A JavaScript decryption binding.
 *
 * This binds a JavaScript function that decrypts a buffer in to a string.
 * The function takes four arguments. The first three are required and
 * are the buffer to decrypt, the key, and the initialisation vector. 
 * The last argument is the decryption type. It defaults to AES 256. The
 * return value is a string containing the decrypted data. It is the 
 * responsibility of the caller to ensure it is printable.
 *
 * @param ctx the duktape context
 * @return an error status or 0.
 */
duk_ret_t do_decryptToString(duk_context * ctx)
{
    duk_ret_t ret = -1;
    duk_int_t count = duk_get_top(ctx);

    /* Must have at least three arguments */
    if (count < 3)
    {
        /* This does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Insufficient arguments!");
    }
    else
    {
        // TODO: Implement real encryption
        if (!duk_is_buffer_data(ctx, 0))
        {
            /* This does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Argument 1 is not a buffer!");
        }
        else
        {
            // For now return the buffer data as a string.
            size_t len = 0;
            void * str = duk_get_buffer_data(ctx, 0, &len);

            duk_push_lstring(ctx, (char*)str, len);

            ret = 1;
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
duk_int_t jse_bind_crypt(jse_context_t * jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_ENTER("jse_bind_crypt(%p)", jse_ctx)

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
                duk_push_c_function(jse_ctx->ctx, do_encrypt, DUK_VARARGS);
                duk_put_global_string(jse_ctx->ctx, "encrypt");

                duk_push_c_function(jse_ctx->ctx, do_decryptToBuffer, DUK_VARARGS);
                duk_put_global_string(jse_ctx->ctx, "decryptToBuffer");

                duk_push_c_function(jse_ctx->ctx, do_decryptToString, DUK_VARARGS);
                duk_put_global_string(jse_ctx->ctx, "decryptToString");
            }

            ref_count ++;
            ret = 0;
        }
    }

    JSE_EXIT("jse_bind_crypt()=%d", ret)
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
void jse_unbind_crypt(jse_context_t * jse_ctx)
{
    JSE_ENTER("jse_unbind_crypt(%p)", jse_ctx)

    ref_count --;
    JSE_VERBOSE("ref_count=%d", ref_count)

    /* TODO: Actually unbind */

    jse_unbind_jserror(jse_ctx);

    JSE_EXIT("jse_unbind_crypt()")
}
