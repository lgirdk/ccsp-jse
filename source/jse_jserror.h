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

#ifndef JSE_JSERROR_H
#define JSE_JSERROR_H

#include "jse_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Throws a base Error and outputs debug reporting the error
 *
 * This function causes a JavaScript Error to be thrown. Details of
 * the error are output as debug via the JSE_ERROR debug macro.
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
 * @brief Throws a RangeError and outputs debug reporting the error
 *
 * NEVER RETURNS!
 *
 * @param CTX the duktape context.
 * @param FORMAT var args starting with the format string.
 */
#define JSE_THROW_RANGE_ERROR(CTX, ...) \
    JSE_THROW_ERROR(CTX, DUK_ERR_RANGE_ERROR, __VA_ARGS__)

/**
 * @brief Throws a TypeError and outputs debug reporting the error
 *
 * NEVER RETURNS!
 *
 * @param CTX the duktape context.
 * @param FORMAT var args starting with the format string.
 */
#define JSE_THROW_TYPE_ERROR(CTX, ...) \
    JSE_THROW_ERROR(CTX, DUK_ERR_TYPE_ERROR, __VA_ARGS__)

/**
 * @brief Throws a URIError and outputs debug reporting the error
 *
 * NEVER RETURNS!
 *
 * @param CTX the duktape context.
 * @param FORMAT var args starting with the format string.
 */
#define JSE_THROW_URI_ERROR(CTX, ...) \
    JSE_THROW_ERROR(CTX, DUK_ERR_URI_ERROR, __VA_ARGS__)

/**
 * @brief Throws a PosixError and outputs debug reporting the error
 *
 * NEVER RETURNS!
 *
 * @param ctx the duktape context.
 * @param _errno the POSIX errno.
 * @param format the format string followed by arguments.
 */
void jse_throw_posix_error(duk_context * ctx, int _errno, const char * format, ...);

/**
 * @brief Throws a PosixError and outputs debug reporting the error
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
 * @brief Binds a set of JavaScript error objects
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_jserror(jse_context_t * jse_ctx);

/**
 * @brief Unbinds the JavaScript extensions.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_jserror(jse_context_t * jse_ctx);

#if defined(__cplusplus)
}
#endif

#endif
