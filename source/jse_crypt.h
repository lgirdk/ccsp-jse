#ifndef JSE_JSCRYPT_H
#define JSE_JSCRYPT_H

#include "jse_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Binds a set of JavaScript crypto extensions
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_crypt(jse_context_t * jse_ctx);

/**
 * Unbinds the JavaScript crypto extensions.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_crypt(jse_context_t * jse_ctx);

#if defined(__cplusplus)
}
#endif

#endif
