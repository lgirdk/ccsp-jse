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
#ifndef CCSP_DUKTAPE_INTERNAL_H
#define CCSP_DUKTAPE_INTERNAL_H

#include <duktape.h>

#define RETURN_LSTRING(res, len) { duk_push_lstring(ctx, res, len); return 1; }
#define RETURN_STRING(res) { duk_push_string(ctx, res); return 1; }
#define RETURN_TRUE { duk_push_true(ctx); return 1; }
#define RETURN_FALSE { duk_push_false(ctx); return 1; }
#define RETURN_LONG(res) { duk_push_number(ctx, res); return 1; }

void init_logger();
void CosaPhpExtLog(const char* format, ...);
int parse_parameter(const char* func, duk_context *ctx, const char* types, ...);
int read_file(const char *filename, char** bufout, size_t* lenout);

#endif
