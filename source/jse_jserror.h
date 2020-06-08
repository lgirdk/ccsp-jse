#ifndef JSE_JSERROR_H
#define JSE_JSERROR_H

#include "jse_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

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
