#ifndef JSE_COSA_H
#define JSE_COSA_H

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Initialise CCSP message bus
 *
 * @return an error status or 0.
 */
int jse_cosa_init();

/**
 * @brief Bind CCSP functions
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_cosa(jse_context_t* jse_ctx);

/**
 * Unbinds the JavaScript extensions.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_cosa(jse_context_t * jse_ctx);

#if defined(__cplusplus)
}
#endif

#endif
