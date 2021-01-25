/*
 If not stated otherwise in this file or this component's Licenses.txt file the
 following copyright and licenses apply:

 Copyright 2018 RDK Management

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#include "jst_internal.h"

duk_ret_t ccsp_cosa_module_open(duk_context *ctx);
duk_ret_t ccsp_session_module_open(duk_context *ctx);
duk_ret_t ccsp_post_module_open(duk_context *ctx);
duk_ret_t ccsp_functions_module_open(duk_context *ctx);

duk_ret_t ccsp_extensions_load(duk_context *ctx)
{
  init_logger();

#ifdef BUILD_RDK
  duk_push_c_function(ctx, ccsp_cosa_module_open, 0);
  duk_call(ctx, 0);
  duk_put_global_string(ctx, "ccsp_cosa");
#endif

  duk_push_c_function(ctx, ccsp_session_module_open, 0);
  duk_call(ctx, 0);
  duk_put_global_string(ctx, "ccsp_session");

  duk_push_c_function(ctx, ccsp_post_module_open, 0);
  duk_call(ctx, 0);
  duk_put_global_string(ctx, "ccsp_post");

  duk_push_c_function(ctx, ccsp_functions_module_open, 0);
  duk_call(ctx, 0);
  duk_put_global_string(ctx, "ccsp");

  return 1;
}

duk_ret_t ccsp_extensions_unload(duk_context *ctx)
{
  (void)ctx;
  return 1;
}
