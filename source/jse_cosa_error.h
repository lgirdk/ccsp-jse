#ifndef JSE_COSA_ERROR_H
#define JSE_COSA_ERROR_H

#include "jse_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Throws a CosaError object.
 *
 * @param ctx the duktape context.
 * @param error_code the Cosa error code.
 * @param format the format string followed by arguments.
 */
void jse_throw_cosa_error(duk_context * ctx, int error_code, const char * format, ...);

/**
 * Throws a CosaError with debug.
 *
 * NEVER RETURNS!
 *
 * @param CTX the duktape context.
 * @param ERRORCODE the Cosa error code.
 * @param FORMAT var args starting with the format string.
 */
#define JSE_THROW_COSA_ERROR(CTX, ERRORCODE, ...) \
    JSE_ERROR(__VA_ARGS__) \
    jse_throw_cosa_error(CTX, ERRORCODE, __VA_ARGS__)

/**
 * Binds a set of JavaScript extensions
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_cosa_error(jse_context_t * jse_ctx);

#if defined(__cplusplus)
}
#endif

#endif
