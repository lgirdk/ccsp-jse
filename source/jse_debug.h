
#ifndef JSE_DEBUG_H
#define JSE_DEBUG_H

#if defined(__cplusplus)
extern "C" {
#endif

#define JSE_DEBUG_ENABLED 1

#define JSE_DEBUG_LEVEL_ERROR    0
#define JSE_DEBUG_LEVEL_WARNING  1
#define JSE_DEBUG_LEVEL_INFO     2
#define JSE_DEBUG_LEVEL_DEBUG    3
#define JSE_DEBUG_LEVEL_VERBOSE  4

extern int jse_verbosity;

#if defined(JSE_DEBUG_ENABLED)

#define JSE_DEBUG_INIT(...)

void jse_debug(char* file, int line, int level, const char* format, ...);

#define JSE_ERROR(...) \
    do { \
        jse_debug(__FILE__, __LINE__, JSE_DEBUG_LEVEL_ERROR, __VA_ARGS__); \
    } while(0);

#define JSE_WARNING(...) \
    if (jse_verbosity >= JSE_DEBUG_LEVEL_WARNING) { \
        jse_debug(__FILE__, __LINE__, JSE_DEBUG_LEVEL_WARNING, __VA_ARGS__); \
    }

#define JSE_INFO(...) \
    if (jse_verbosity >= JSE_DEBUG_LEVEL_INFO) { \
        jse_debug(__FILE__, __LINE__, JSE_DEBUG_LEVEL_INFO, __VA_ARGS__); \
    }

#define JSE_DEBUG(...) \
    if (jse_verbosity >= JSE_DEBUG_LEVEL_DEBUG) { \
        jse_debug(__FILE__, __LINE__, JSE_DEBUG_LEVEL_DEBUG, __VA_ARGS__); \
    }

#define JSE_VERBOSE(...) \
    if (jse_verbosity >= JSE_DEBUG_LEVEL_VERBOSE) { \
        jse_debug(__FILE__, __LINE__, JSE_DEBUG_LEVEL_VERBOSE, __VA_ARGS__); \
    }

#define JSE_ASSERT(EXP) \
    if (!(EXP)) { \
        jse_debug(__FILE__, __LINE__, JSE_DEBUG_LEVEL_ERROR, "ASSERT: " #EXP); \
    }

#else

#define JSE_DEBUG_INIT(...)

#define JSE_ERROR(...)
#define JSE_WARNING(...)
#define JSE_INFO(...)
#define JSE_DEBUG(...)
#define JSE_VERBOSE(...)
#define JSE_ASSERT(EXP)
	
#endif

#if defined(__cplusplus)
}
#endif

#endif