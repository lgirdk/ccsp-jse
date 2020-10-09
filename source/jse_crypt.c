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

#include <openssl/evp.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "jse_debug.h"
#include "jse_jserror.h"
#include "jse_crypt.h"

/** Default crypt error */
#define ERROR_CRYPT_DEFAULT -1
/** Error if the key length is incorrect */
#define ERROR_CRYPT_INVALID_KEY_LENGTH -2
/** Error if the IV is missing */
#define ERROR_CRYPT_IV_MISSING -3
/** Error if the IV length is incorrect */
#define ERROR_CRYPT_INVALID_IV_LENGTH -4

/** Reference count for binding. */
static int ref_count = 0;

/**
 * Returns the cipher engine based upon the index
 *
 * @param idx the index
 * @return the engine or NULL if idx is invalid
 */
static const EVP_CIPHER * getCipher(size_t idx)
{
    const EVP_CIPHER * ciphers[] =
    {
       EVP_aes_128_cbc(),
       EVP_aes_128_ecb(),
       EVP_aes_128_cfb(),
       EVP_aes_128_ofb(),
       EVP_aes_192_cbc(),
       EVP_aes_192_ecb(),
       EVP_aes_192_cfb(),
       EVP_aes_192_ofb(),
       EVP_aes_256_cbc(),
       EVP_aes_256_ecb(),
       EVP_aes_256_cfb(),
       EVP_aes_256_ofb(),
       EVP_des_cbc(),
       EVP_des_ecb(),
       EVP_des_cfb(),
       EVP_des_ofb(),
       EVP_des_ede_cbc(),
       EVP_des_ede(),
       EVP_des_ede_ofb(),
       EVP_des_ede_cfb(),
       EVP_des_ede3_cbc(),
       EVP_des_ede3(),
       EVP_des_ede3_ofb(),
       EVP_des_ede3_cfb(),
    };

    const EVP_CIPHER * cipher = NULL;

    if (idx < (sizeof ciphers / sizeof ciphers[0]))
    {
        cipher = ciphers[idx];
    }

    return cipher;
}

/**
 * Throws an appropriate error based on a (en/de)crypt error.
 *
 * THIS FUNCTION NEVER RETURNS!
 *
 * @oaram ctx the duktape context
 * @param error the error code
 * @param name the function name
 */
static void throwCryptError(duk_context * ctx, int error, const char * name)
{
    switch (error)
    {
        case ERROR_CRYPT_INVALID_KEY_LENGTH:
            /* Does not return */
            JSE_THROW_RANGE_ERROR(ctx, "Invalid key length");
            break;
        case ERROR_CRYPT_IV_MISSING:
            /* Does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Missing IV parameter");
            break;
        case ERROR_CRYPT_INVALID_IV_LENGTH:
            /* Does not return */
            JSE_THROW_RANGE_ERROR(ctx, "Invalid IV length");
            break;
        default:
            JSE_THROW_ERROR(ctx, DUK_ERR_ERROR, "%s() failed", name);
            break;
    }
}

/**
 * Encrypts an input buffer
 *
 * This method encrypts inlen bytes of the input buffer pointed to by inbuf.
 * It returns a pointer to a new buffer, containing the encrypted data, in
 * poutbuf and its size in poutlen. A pointer to the cipher engine is
 * specified in cipher. The key is pointed to by key and the optional IV
 * is pointed to by iv.
 *
 * The following errors are returned:
 *
 *   ERROR_CRYPT_INVALID_KEY_LENGTH - the key length is invalid for the
 *                                    cipher.
 *   ERROR_CRYPT_IV_MISSING         - the cipher requires an IV.
 *   ERROR_CRYPT_INVALID_IV_LENGTH  - the IV length is invalid for the
 *                                    cipher.
 *   ERROR_CRYPT_DEFAULT            - any other error.
 *
 * @param inbuf a pointer to the input buffer
 * @param inlen the size of the input data
 * @param poutbuf a pointer to return a pointer to the output buffer
 * @param poutlen a pointer to return the size of the output data
 * @param cipher a pointer to the cipher function
 * @param key a pointer to the key
 * @param keylen the length of the key
 * @param iv a pointer to the IV or NULL
 * @param ivlen the length of the IV if iv is not NULL
 *
 * @return 0 on success or an error value
 */
static int encrypt(
    const void * inbuf, size_t inlen, void ** poutbuf, size_t * poutlen,
    const EVP_CIPHER * cipher, const void * key, size_t keylen,
    const void * iv, size_t ivlen)
{
    int ret = -1;

    EVP_CIPHER_CTX * ctx = EVP_CIPHER_CTX_new();
    if (ctx != NULL)
    {
        unsigned char * outstr = (unsigned char*)calloc(inlen + EVP_MAX_BLOCK_LENGTH, sizeof(char));
        if (outstr != NULL)
        {
            const unsigned char * instr = (const unsigned char*)inbuf;
            int outlen = 0;

            EVP_CIPHER_CTX_init(ctx);

            /* Set up encryption without setting the key or iv */
            EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL);

            /* Test the key */
            if (EVP_CIPHER_CTX_key_length(ctx) != (int)keylen)
            {
                JSE_ERROR("Invalid key length! %d bytes required!", EVP_CIPHER_CTX_key_length(ctx))
                ret = ERROR_CRYPT_INVALID_KEY_LENGTH;
            }
            else
            if (EVP_CIPHER_CTX_iv_length(ctx) != 0 &&
                (iv == NULL || (EVP_CIPHER_CTX_iv_length(ctx) != (int)ivlen)))
            {
                if (iv == NULL)
                {
                    JSE_ERROR("Missing IV!")
                    ret = ERROR_CRYPT_IV_MISSING;
                }
                else
                {
                    JSE_ERROR("Invalid IV length: %d (%d bytes required)",
                        strlen(iv), EVP_CIPHER_CTX_iv_length(ctx))
                    ret = ERROR_CRYPT_INVALID_IV_LENGTH;
                }
            }
            else
            {
                /* Update the key and IV values */
                EVP_EncryptInit_ex(ctx, NULL, NULL,
                    (unsigned const char *)key, (unsigned const char *)iv);

                if (EVP_EncryptUpdate(ctx, outstr, &outlen, instr, (int)inlen))
                {
                    int extralen = 0;
                    if (EVP_EncryptFinal_ex(ctx, outstr + outlen, &extralen))
                    {
                        *poutbuf = outstr;
                        *poutlen = (size_t)(outlen + extralen);
                        ret = 0;
                    }
                    else
                    {
                        JSE_ERROR("EVP_EncryptFinal_ex() failed!")
                    }
                }
                else
                {
                    JSE_ERROR("EVP_EncryptUpdate() failed!")
                }
            }

            EVP_CIPHER_CTX_free(ctx);

            /* Free the output block on error */
            if (ret != 0)
            {
                free(outstr);
            }
        }
        else
        {
            JSE_ERROR("calloc() failed: %s", strerror(errno))
        }
    }
    else
    {
        JSE_ERROR("Could not create cypher context!");
    }

    return ret;
}

/**
 * Decrypts an input buffer
 *
 * This method decrypts inlen bytes of the input buffer pointed to by inbuf.
 * It returns a pointer to a new buffer, containing the decrypted data, in
 * poutbuf and its size in poutlen. A pointer to the cipher engine is
 * specified in cipher. The key is pointed to by key and the optional IV
 * is pointed to by iv.
 *
 * The following errors are returned:
 *
 *   ERROR_CRYPT_INVALID_KEY_LENGTH - the key length is invalid for the
 *                                    cipher.
 *   ERROR_CRYPT_IV_MISSING         - the cipher requires an IV.
 *   ERROR_CRYPT_INVALID_IV_LENGTH  - the IV length is invalid for the
 *                                    cipher.
 *   ERROR_CRYPT_DEFAULT            - any other error.
 *
 * @param inbuf a pointer to the input bufferThe first three are required and
 * are the buffer to decrypt, the key, and the initialisation vector.
 * The last argument is the decryption type. It defaults to AES 256 CBC.
 * @param inlen the size of the input data
 * @param poutbuf a pointer to return a pointer to the output buffer
 * @param poutlen a pointer to return the size of the output data
 * @param cipher a pointer to the cipher function
 * @param key a pointer to the key
 * @param keylen the length of the key
 * @param iv a pointer to the IV or NULL
 * @param ivlen the length of the IV if iv is not NULL
*
 * @return 0 on success or an error value
 */
static int decrypt(
    const void * inbuf, size_t inlen, void ** poutbuf, size_t * poutlen,
    const EVP_CIPHER * cipher, const void * key, size_t keylen,
    const void * iv, size_t ivlen)
{
    int ret = -1;

    EVP_CIPHER_CTX * ctx = EVP_CIPHER_CTX_new();
    if (ctx != NULL)
    {
        unsigned char * outstr = (unsigned char*)calloc(inlen + EVP_MAX_BLOCK_LENGTH, sizeof(char));
        if (outstr != NULL)
        {
            const unsigned char * instr = (const unsigned char*)inbuf;
            int outlen = 0;

            EVP_CIPHER_CTX_init(ctx);

            /* Set up encryption without setting the key or iv */
            EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL);

            /* Test the key */
            if (EVP_CIPHER_CTX_key_length(ctx) != (int)keylen)
            {
                JSE_ERROR("Invalid key length! %d bytes required!", EVP_CIPHER_CTX_key_length(ctx))
                ret = ERROR_CRYPT_INVALID_KEY_LENGTH;
            }
            else
            if (EVP_CIPHER_CTX_iv_length(ctx) != 0 &&
                (iv == NULL || (EVP_CIPHER_CTX_iv_length(ctx) != (int)ivlen)))
            {
                if (iv == NULL)
                {
                    JSE_ERROR("Missing IV!")
                    ret = ERROR_CRYPT_IV_MISSING;
                }
                else
                {
                    JSE_ERROR("Invalid IV length: %d (%d bytes required)",
                        strlen(iv), EVP_CIPHER_CTX_iv_length(ctx))
                    ret = ERROR_CRYPT_INVALID_IV_LENGTH;
                }
            }
            else
            {
                /* Update the key and IV values */
                EVP_DecryptInit_ex(ctx, NULL, NULL, (unsigned char *)key, (unsigned char *)iv);

                if (EVP_DecryptUpdate(ctx, outstr, &outlen, instr, (int)inlen))
                {
                    int extralen = 0;
                    if (EVP_DecryptFinal_ex(ctx, outstr + outlen, &extralen))
                    {
                        *poutbuf = outstr;
                        *poutlen = (size_t)(outlen + extralen);
                        ret = 0;
                    }
                    else
                    {
                        JSE_ERROR("EVP_DecryptFinal_ex() failed!")
                    }
                }
                else
                {
                    JSE_ERROR("EVP_DecryptUpdate() failed!")
                }
            }

            EVP_CIPHER_CTX_free(ctx);

            /* Free the output block on error */
            if (ret != 0)
            {
                free(outstr);
            }
        }
        else
        {
            JSE_ERROR("calloc() failed: %s", strerror(errno))
        }
    }
    else
    {
        JSE_ERROR("Could not create cypher context!");
    }

    return ret;
}

/**
 * A JavaScript encryption binding.
 *
 * This binds a JavaScript function that encrypts a value in to a buffer.
 * The parameters passed are:
 *
 * 0 - the buffer to decrypt
 * 1 - the key (must be the right length for the encryption algorithm)
 * 2 - the encryption method
 * 3 - the IV (Not needed for some encryption algorithms)
 *
 * The return value is a buffer object containing the encrypted data.
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
        const EVP_CIPHER * cipher = NULL;
        const void * key = NULL;
        size_t keylen = 0;
        const void * iv = NULL;
        size_t ivlen = 0;
        const void * inbuf = NULL;
        size_t inlen = 0;
        void * outbuf = NULL;
        size_t outlen = 0;
        int encret = 0;

        if (duk_is_buffer_data(ctx, 1))
        {
            key = duk_get_buffer_data(ctx, 1, &keylen);
        }
        else
        {
            key = duk_safe_to_string(ctx, 1);
            keylen = strlen(key);
        }

        if (!duk_is_number(ctx, 2))
        {
            /* Does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Third argument must be an integer!");
        }
        else
        {
            cipher = getCipher(duk_get_uint(ctx, 2));
            if (cipher == NULL)
            {
                /* Does not return */
                JSE_THROW_RANGE_ERROR(ctx, "Invalid cipher index!");
            }
        }

        /* IV value only needed for some encryption */
        if (count > 3)
        {
            if (duk_is_buffer_data(ctx, 3))
            {
                iv = duk_get_buffer_data(ctx, 3, &ivlen);
            }
            else
            {
                iv = duk_safe_to_string(ctx, 3);
                ivlen = strlen(iv);
            }
        }

        if (duk_is_buffer_data(ctx, 0))
        {
            inbuf = duk_get_buffer_data(ctx, 0, &inlen);
        }
        else
        {
            inbuf = duk_safe_to_string(ctx, 0);
            inlen = strlen(inbuf);
        }

        encret = encrypt(inbuf, inlen, &outbuf, &outlen, cipher, key, keylen, iv, ivlen);
        if (encret != 0)
        {
            throwCryptError(ctx, encret, "encrypt");
        }
        else
        {
            void * retbuf = duk_push_fixed_buffer(ctx, outlen);
            memcpy(retbuf, outbuf, outlen);
            free(outbuf);

            ret = 1;
        }
    }

    return ret;
}

/**
 * The core of JavaScript decryption binding.
 *
 * This implements the duktape parameter parsing and validation before
 * calling decrypt() to decrypt a buffer in to plaintext. The parameters
 * passed are:
 *
 * 0 - the buffer to decrypt
 * 1 - the key (must be the right length for the encryption algorithm)
 * 2 - the encryption method
 * 3 - the IV (Not needed for some encryption algorithms)
 *
 * @param ctx the duktape context
 * @param asstring true to return a string, otherwise a buffer
 * @return an error status or 0.
 */
duk_ret_t do_decrypt(duk_context * ctx, bool asstring)
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
        if (!duk_is_buffer_data(ctx, 0))
        {
            /* Does not return */
            JSE_THROW_TYPE_ERROR(ctx, "First argument is not a buffer!");
        }
        else
        {
            size_t inlen = 0;
            const EVP_CIPHER * cipher = NULL;
            const void * inbuf = duk_get_buffer_data(ctx, 0, &inlen);
            const void * key = NULL;
            size_t keylen = 0;
            const void * iv = NULL;
            size_t ivlen = 0;
            void * outbuf = NULL;
            size_t outlen = 0;
            int decret = 0;

            if (duk_is_buffer_data(ctx, 1))
            {
                key = duk_get_buffer_data(ctx, 1, &keylen);
            }
            else
            {
                key = duk_safe_to_string(ctx, 1);
                keylen = strlen(key);
            }

            if (!duk_is_number(ctx, 2))
            {
                /* Does not return */
                JSE_THROW_TYPE_ERROR(ctx, "Third argument must be an integer!");
            }
            else
            {
                cipher = getCipher(duk_get_uint(ctx, 2));
                if (cipher == NULL)
                {
                    /* Does not return */
                    JSE_THROW_RANGE_ERROR(ctx, "Invalid cipher index!");
                }
            }

            /* IV value only needed for some encryption */
            if (count > 3)
            {
                if (duk_is_buffer_data(ctx, 3))
                {
                    iv = duk_get_buffer_data(ctx, 3, &ivlen);
                }
                else
                {
                    iv = duk_safe_to_string(ctx, 3);
                    ivlen = strlen(iv);
                }
            }

            decret = decrypt(inbuf, inlen, &outbuf, &outlen, cipher, key, keylen, iv, ivlen);
            if (decret != 0)
            {
                throwCryptError(ctx, decret, "decrypt");
            }
            else
            {
                if (asstring)
                {
                    duk_push_lstring(ctx, outbuf, outlen);
                }
                else
                {
                    void * dukbuf = duk_push_fixed_buffer(ctx, outlen);
                    memcpy(dukbuf, outbuf, outlen);
                }

                free(outbuf);

                ret = 1;
            }
        }
    }

    return ret;
}

/**
 * A JavaScript decryption binding.
 *
 * This binds a JavaScript function that decrypts a buffer in to a string.
 * The function takes four arguments. The parameters passed are:
 *
 * 0 - the buffer to decrypt
 * 1 - the key (must be the right length for the encryption algorithm)
 * 2 - the encryption method
 * 3 - the IV (Not needed for some encryption algorithms)
 *
 * The return value is a string containing the decrypted data. It is the
 * responsibility of the caller to ensure it is printable.
 *
 * @param ctx the duktape context
 * @return an error status or 0.
 */
duk_ret_t do_decryptToString(duk_context * ctx)
{
    duk_ret_t ret = -1;

    ret = do_decrypt(ctx, true);

    return ret;
}

/**
 * A JavaScript decryption binding.
 *
 * This binds a JavaScript function that decrypts a buffer in to a buffer.
 * The function takes four arguments.  The parameters passed are:
 *
 * 0 - the buffer to decrypt
 * 1 - the key (must be the right length for the encryption algorithm)
 * 2 - the encryption method
 * 3 - the IV (Not needed for some encryption algorithms)
 *
 * The return value is a buffer object containing the decrypted data.
 *
 * @param ctx the duktape context
 * @return an error status or 0.
 */
duk_ret_t do_decryptToBuffer(duk_context * ctx)
{
    duk_ret_t ret = -1;

    ret = do_decrypt(ctx, false);

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
    if (jse_ctx != NULL && jse_ctx->ctx != NULL)
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
