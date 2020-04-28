
#ifdef ENABLE_FASTCGI
#include "fcgi_stdio.h"
#else
#include <stdio.h>
#endif
#include <stdarg.h>
#include <string.h>

#include "jse_debug.h"

int jse_verbosity = 0;

void jse_debug(char *file, int line, int level, const char *format, ...)
{
    char buffer[1024];
    va_list ap;
    int len;

    va_start(ap, format);
    len = vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    if (len >= 0 && len < (int)sizeof(buffer))
    {
        switch (level)
        {
            case JSE_DEBUG_LEVEL_ERROR:
                fprintf(stderr, "%s:%d:ERROR:%s\n", file, line, buffer);
                break;
            case JSE_DEBUG_LEVEL_WARNING:
                fprintf(stderr, "%s:%d:WARNING:%s\n", file, line, buffer);
                break;
            case JSE_DEBUG_LEVEL_INFO:
                fprintf(stderr, "%s:%d:INFO:%s\n", file, line, buffer);
                break;
            case JSE_DEBUG_LEVEL_DEBUG:
                fprintf(stderr, "%s:%d:DEBUG:%s\n", file, line, buffer);
                break;
            case JSE_DEBUG_LEVEL_VERBOSE:
                fprintf(stderr, "%s:%d:VERBOSE:%s\n", file, line, buffer);
                break;
            default:
                break;
        }
    }
}
