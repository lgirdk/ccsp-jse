
#include <duktape.h>
#include "jse_debug.h"
#include "jse_jserror.h"

/**
 * Binds a set of JavaScript error objects
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_jserror(jse_context_t * jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_VERBOSE("Binding JS errors!")

    if (jse_ctx != NULL)
    {
        ret = 0;
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}
