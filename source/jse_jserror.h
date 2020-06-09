#ifndef JSE_JSERROR_H
#define JSE_JSERROR_H

#include "jse_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Lots of time you want to report on debug and throw an exception with the
 * same message. This macro does that.
 *
 * NEVER RETURNS!
 *
 * @param CTX the duktape context.
 * @param TYPE the error type.
 * @param FORMAT the format string followed by arguments.
 */
#define JSE_THROW_ERROR(CTX, TYPE, ...) \
    JSE_ERROR(__VA_ARGS__) \
    (void) duk_error(CTX, TYPE, __VA_ARGS__)

/**
 * Throws a RangeError with debug.
 *
 * NEVER RETURNS!
 *
 * @param CTX the duktape context.
 * @param FORMAT var args starting with the format string.
 */
#define JSE_THROW_RANGE_ERROR(CTX, ...) \
    JSE_THROW_ERROR(CTX, DUK_ERR_RANGE_ERROR, __VA_ARGS__)

/**
 * Throws a TypeError with debug.
 *
 * NEVER RETURNS!
 *
 * @param CTX the duktape context.
 * @param FORMAT var args starting with the format string.
 */
#define JSE_THROW_TYPE_ERROR(CTX, ...) \
    JSE_THROW_ERROR(CTX, DUK_ERR_TYPE_ERROR, __VA_ARGS__)

/**
 * Throws a URIError with debug.
 *
 * NEVER RETURNS!
 *
 * @param CTX the duktape context.
 * @param FORMAT var args starting with the format string.
 */
#define JSE_THROW_URI_ERROR(CTX, ...) \
    JSE_THROW_ERROR(CTX, DUK_ERR_URI_ERROR, __VA_ARGS__)

/**
 * Throws a PosixError object.
 *
 * @param ctx the duktape context.
 * @param _errno the POSIX errno.
 * @param format the format string followed by arguments.
 */
void jse_throw_posix_error(duk_context * ctx, int _errno, const char * format, ...);

/**
 * Throws a PosixError with debug.
 *
 * NEVER RETURNS!
 *
 * @param CTX the duktape context.
 * @param ERRNO the POSIX errno.
 * @param FORMAT var args starting with the format string.
 */
#define JSE_THROW_POSIX_ERROR(CTX, ERRNO, ...) \
    JSE_ERROR(__VA_ARGS__) \
    jse_throw_posix_error(CTX, ERRNO, __VA_ARGS__)

/**
 * Binds a set of JavaScript error objects
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_jserror(jse_context_t * jse_ctx);

#if defined(__cplusplus)
}
#endif

#endif
