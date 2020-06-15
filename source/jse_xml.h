#ifndef JSE_XML_H
#define JSE_XML_H

#include "jse_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Binds a set of JavaScript extensions relating to XML manipulation
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_xml(jse_context_t *jse_ctx);

/**
 * Unbinds the JavaScript extensions.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_xml(jse_context_t * jse_ctx);

#if defined(__cplusplus)
}
#endif

#endif
