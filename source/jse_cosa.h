/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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
*/

#ifndef JSE_COSA_H
#define JSE_COSA_H

#include "jse_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Initialise CCSP message bus
 *
 * @return an error status or 0.
 */
int jse_cosa_init(void);

/**
 * Shutdown the CCSP message bus
 */
void jse_cosa_shutdown(void);

/**
 * @brief Bind CCSP functions
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_cosa(jse_context_t* jse_ctx);

/**
 * @brief Unbinds the JavaScript extensions.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_cosa(jse_context_t * jse_ctx);

#if defined(__cplusplus)
}
#endif

#endif
