
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
