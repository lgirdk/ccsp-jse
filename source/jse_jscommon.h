
#ifndef JSE_JSCOMMON_H
#define JSE_JSCOMMON_H

#include "jse_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Runs JavaScript code stored in a buffer.
 *
 * @param jse_ctx the jse context.
 * @param buffer the buffer.
 * @param size the size of the context.
 * @return an error status or 0.
 */
duk_ret_t jse_run_buffer(jse_context_t *jse_ctx, const char* buffer, size_t size);

/**
 * Binds a set of JavaScript extensions
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_jscommon(jse_context_t * jse_ctx);

/**
 * Unbinds the JavaScript extensions.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_jscommon(jse_context_t * jse_ctx);

#if defined(__cplusplus)
}
#endif

#endif
