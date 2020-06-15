
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "jse_debug.h"
#include "jse_common.h"
#include "jse_jsprocess.h"
#include "jse_jserror.h"

/** Reference count for binding. */
static int ref_count = 0;

/**
 * The binding for sendSignal()
 * 
 * This function sends a signal to a process. It takes two areguments,
 * the first is required and is the PID of the process. The second is the
 * signal. If the signal is omitted SIGTERM is sent.
 * 
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
duk_ret_t do_send_signal(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    duk_idx_t num = duk_get_top(ctx);
    int sig = SIGTERM;
    int pid = 0;

    if (num == 0)
    {
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Insufficient arguments!");
    }
    else
    {
        pid = duk_get_int_default(ctx, 0, 0);
        if (pid == 0)
        {
            /* Does not return */
            JSE_THROW_RANGE_ERROR(ctx, "Invalid pid (%d)", pid);
        }

        if (num > 1)
        {
            sig = duk_get_int_default(ctx, 1, SIGTERM);
        }

        JSE_VERBOSE("pid=%d, sig=%d", pid, sig)

        if (kill(pid, sig) == -1)
        {
            JSE_THROW_POSIX_ERROR(ctx, errno, "kill() failed: %s", strerror(errno));
        }

        ret = 0;
    }

    return ret;
}

/**
 * Binds a set of JavaScript extensions
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_jsprocess(jse_context_t * jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_VERBOSE("Binding JS common!")

    JSE_VERBOSE("ref_count=%d", ref_count)
    if (jse_ctx != NULL)
    {
        /* jscommon is dependent upon jserror error objects so bind here */
        if ((ret = jse_bind_jserror(jse_ctx)) != 0)
        {
            JSE_ERROR("Failed to bind JS error objects")
        }
        else
        {
            if (ref_count == 0)
            {
                duk_push_c_function(jse_ctx->ctx, do_send_signal, DUK_VARARGS);
                duk_put_global_string(jse_ctx->ctx, "sendSignal");
            }

            ref_count ++;
            ret = 0;
        }
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Unbinds the JavaScript extensions.
 *
 * Actually just decrements the reference count. Needed for fast cgi
 * since the same process will rebind. Not unbinding is not an issue
 * as the duktape context is destroyed each time cleaning everything
 * up.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_jsprocess(jse_context_t * jse_ctx)
{
    ref_count --;
    JSE_VERBOSE("ref_count=%d", ref_count)

    /* TODO: Actually unbind */

    jse_unbind_jserror(jse_ctx);
}
