
#include <sys/time.h>
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
        switch (level)
        {
            case JSE_DEBUG_LEVEL_ERROR:
                fprintf(stderr, "%08ld.%03ld:%s:%d:ERROR:%s\n", (long int)tv.tv_sec, ms, file, line, buffer);
                break;
            case JSE_DEBUG_LEVEL_WARNING:
                fprintf(stderr, "%08ld.%03ld:%s:%d:WARNING:%s\n", (long int)tv.tv_sec, ms, file, line, buffer);
                break;
            case JSE_DEBUG_LEVEL_INFO:
                fprintf(stderr, "%08ld.%03ld:%s:%d:INFO:%s\n", (long int)tv.tv_sec, ms, file, line, buffer);
                break;
            case JSE_DEBUG_LEVEL_DEBUG:
                fprintf(stderr, "%08ld.%03ld:%s:%d:DEBUG:%s\n", (long int)tv.tv_sec, ms, file, line, buffer);
                break;
            case JSE_DEBUG_LEVEL_VERBOSE:
                fprintf(stderr, "%08ld.%03ld:%s:%d:VERBOSE:%s\n", (long int)tv.tv_sec, ms, file, line, buffer);
                break;
            default:
                break;
        }
    }
}
