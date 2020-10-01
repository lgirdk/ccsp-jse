
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

#include <sys/time.h>
#ifdef ENABLE_FASTCGI
#include "fcgi_stdio.h"
#else
#include <stdio.h>
#endif
#include <stdarg.h>
#include <string.h>

#include "jse_debug.h"

/** The verbosity level */
int jse_verbosity = 0;

/** The enter exit debug flag */
bool jse_enter_exit = false;

/**
 * Outputs a line of debug.
 *
 * @param file the source file name
 * @param line the source line number
 * @param levelStr the level as a string
 * @param format a printf() style formatting string
 * @param ... further printf() style arguments
 */
void jse_debugPrint(char *file, int line, const char* levelStr, const char *format, ...)
{
    struct timeval tv;
    long int ms;
    char buffer[1024];
    va_list ap;
    int len;

    (void) gettimeofday(&tv, NULL);
    ms = (long int)tv.tv_usec / 1000;

    va_start(ap, format);
    len = vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    if (len >= 0 && len < (int)sizeof(buffer))
    {
        fprintf(stderr, "%08ld.%03ld:%s:%d:%s:%s\n", (long int)tv.tv_sec, ms, file, line, levelStr, buffer);
    }
}

/**
 * Returns the textual debug level for an integer debug level.
 *
 * @param level the debug level
 * @return the textual debug level
 */
const char* jse_debugGetLevel(int level)
{
    static const char* levels[] =
    {
        "ERROR",
        "WARNING",
        "INFO",
        "DEBUG",
        "VERBOSE"
    };

    if (level > JSE_DEBUG_LEVEL_MAX)
    {
        return "UNKNOWN";
    }

    return levels[level];
}
