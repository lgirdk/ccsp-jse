#ifndef JSE_DEBUG_H
#define JSE_DEBUG_H

#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define JSE_DEBUG_ENABLED 1

#define JSE_DEBUG_LEVEL_ERROR    0
#define JSE_DEBUG_LEVEL_WARNING  1
#define JSE_DEBUG_LEVEL_INFO     2
#define JSE_DEBUG_LEVEL_DEBUG    3
#define JSE_DEBUG_LEVEL_VERBOSE  4
#define JSE_DEBUG_LEVEL_MAX      JSE_DEBUG_LEVEL_VERBOSE

// These are for internal use
#define JSE_DEBUG_LEVEL_ENTER    5
#define JSE_DEBUG_LEVEL_EXIT     5

/** The debug verbosity level */
extern int jse_verbosity;

/** The enter/exit debug enable */
extern bool jse_enter_exit;

#if defined(JSE_DEBUG_ENABLED)

#define JSE_DEBUG_INIT(...)

/**
 * Outputs a line of debug.
 *
 * @param file the source file name
 * @param line the source line number
 * @param levelStr the level as a string
 * @param format a printf() style formatting string
 * @param ... further printf() style arguments
 */
void jse_debugPrint(char* file, int line, const char* levelStr, const char* format, ...);

/**
 * Returns the textual debug level for an integer debug level.
 *
 * @param level the debug level
 * @return the textual debug level
 */
const char* jse_debugGetLevel(int level);

#define JSE_ERROR(...) \
    do { \
        jse_debugPrint(__FILE__, __LINE__, "ERROR", __VA_ARGS__); \
    } while(0);

#define JSE_WARNING(...) \
    if (jse_verbosity >= JSE_DEBUG_LEVEL_WARNING) { \
        jse_debugPrint(__FILE__, __LINE__, "WARNING", __VA_ARGS__); \
    }

#define JSE_INFO(...) \
    if (jse_verbosity >= JSE_DEBUG_LEVEL_INFO) { \
        jse_debugPrint(__FILE__, __LINE__, "INFO", __VA_ARGS__); \
    }

#define JSE_DEBUG(...) \
    if (jse_verbosity >= JSE_DEBUG_LEVEL_DEBUG) { \
        jse_debugPrint(__FILE__, __LINE__, "DEBUG", __VA_ARGS__); \
    }

#define JSE_VERBOSE(...) \
    if (jse_verbosity >= JSE_DEBUG_LEVEL_VERBOSE) { \
        jse_debugPrint(__FILE__, __LINE__, "VERBOSE", __VA_ARGS__); \
    }

#define JSE_ENTER(...) \
    if (jse_enter_exit) { \
        jse_debugPrint(__FILE__, __LINE__, "ENTER", __VA_ARGS__); \
    }

#define JSE_EXIT(...) \
    if (jse_enter_exit) { \
        jse_debugPrint(__FILE__, __LINE__, "EXIT", __VA_ARGS__); \
    }

#define JSE_ASSERT(EXP) \
    if (!(EXP)) { \
        jse_debugPrint(__FILE__, __LINE__, "ASSERT", #EXP); \
    }

#else

#define JSE_DEBUG_INIT(...)

#define JSE_ERROR(...)
#define JSE_WARNING(...)
#define JSE_INFO(...)
#define JSE_DEBUG(...)
#define JSE_VERBOSE(...)
#define JSE_ENTER(...)
#define JSE_EXIT(...)
#define JSE_ASSERT(EXP)
	
#endif

#if defined(__cplusplus)
}
#endif

#endif
