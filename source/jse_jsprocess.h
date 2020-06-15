
#ifndef JSE_JSPROCESS_H
#define JSE_JSPROCESS_H

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Binds a set of JavaScript extensions
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_jsprocess(jse_context_t * jse_ctx);

/**
 * Unbinds the JavaScript extensions.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_jsprocess(jse_context_t * jse_ctx);

#if defined(__cplusplus)
}
#endif

#endif
