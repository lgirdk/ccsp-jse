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
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_cosa(duk_context *ctx);

#if defined(__cplusplus)
}
#endif

#endif
