##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2019 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
AM_CFLAGS = -D_ANSC_LINUX
AM_CFLAGS += -D_ANSC_USER
AM_CFLAGS += -D_ANSC_LITTLE_ENDIAN_
AM_CFLAGS += -Os -pedantic -std=c99 -Wall -fstrict-aliasing -fomit-frame-pointer -DDUK_USE_PROMISE_BUILTIN
AM_LDFLAGS = -lccsp_common
AM_CPPFLAGS = -Wall -Werror
ACLOCAL_AMFLAGS = -I m4
sbin_PROGRAMS = jst
jst_CPPFLAGS = -DBUILD_RDK
jst_CPPFLAGS += -DDUK_CMDLINE_PRINTALERT_SUPPORT 
jst_CPPFLAGS += -DDUK_CMDLINE_CONSOLE_SUPPORT
jst_CPPFLAGS += -DDUK_CMDLINE_LOGGING_SUPPORT
jst_CPPFLAGS += -DDUK_CMDLINE_MODULE_SUPPORT
jst_CPPFLAGS += -I$(top_srcdir)/source -I$(top_srcdir)/source/duktape $(CPPFLAGS)
jst_SOURCES = jst_parser.c  jst_cosa.c jst_session.c jst_post.c jst_functions.c jst_internal.c jst_extensions.c $(top_srcdir)/source/duktape/duktape.c $(top_srcdir)/source/duktape/duk_cmdline.c $(top_srcdir)/source/duktape/duk_print_alert.c $(top_srcdir)/source/duktape/duk_console.c $(top_srcdir)/source/duktape/duk_logging.c $(top_srcdir)/source/duktape/duk_module_duktape.c
jst_LDFLAGS = -lccsp_common -lm -lcrypto $(LDFLAGS)









