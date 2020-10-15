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

#ifndef JSE_XML_H
#define JSE_XML_H

#include "jse_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Binds a set of JavaScript extensions relating to XML manipulation
 *
 * @param jse_ctx the jse context.
 *
 * @return an error status or 0.
 */
duk_int_t jse_bind_xml(jse_context_t *jse_ctx);

/**
 * @brief Unbinds the JavaScript extensions.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_xml(jse_context_t * jse_ctx);

#if defined(__cplusplus)
}
#endif

#endif
