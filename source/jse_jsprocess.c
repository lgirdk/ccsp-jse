
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

#define max(a, b) ((a > b) ? (a) : (b))

/**
 * Forks the current process and execs filename as a process.
 * 
 * This function opens pipes, forks the current process, it then sets the
 * stdout and stderr of the child process to be one side of the pipes.
 * It this execs filename as the new process. If successful it returns the
 * PID of the new process and sets the variables pointed to by pOutfd and
 * pErrfd to the read file descriptors of the pipes.
 * 
 * The filename should comprise the full path to the process to be executed.
 * The args variable conmprises the command line arguments to the process.
 * The env variable, comprises an array of strings containing NAME=VALUE 
 * pairs which is the environment for the process. Both arrays should be
 * terminated with a NULL value.
 * 
 * If either pOutfd or pErrfd is NULL then stdout or stderr will not be
 * redirected, as appropriate.
 * 
 * @param filename the filename of the new process.
 * @param args an array of command line arguments for the process.
 * @param env an array of key=value environment strings for the process.
 * @param pOutfd a pointer to a variable for the stdout pipe read fd.
 * @param pErrfd a pointer to a variable for the stderr pipe read fd.
 * 
 * @return the PID or -1 on error.
 */ 
static pid_t fork_and_exec(const char * const filename, char * const args[], char * const env[], int * pOutfd, int * pErrfd)
{
    pid_t pid = -1;
    int outfds[2];
    int errfds[2];

    /* TODO: May want to add stdin redirection at some point */

    /* pipe() creates a pipe and returns two file descriptors. fds[0] is
       the read end of the pipe and fds[1] is the write end of the pipe. 
       So to redirect stdout or stderr, you set stdout or stderr to the
       write end of the pipe and read from the read end of the pipe. */
    if (pOutfd == NULL || pipe(outfds) != -1)
    {
        if (pErrfd == NULL || pipe(errfds) != -1)
        {
            pid = fork();
            if (pid == 0) /* We are in the child */
            {
                int ret = -1;

                if (pOutfd != NULL)
                {
                    /* dup2() duplicates the file descriptor provided in the
                       first parameter, closed the file descriptor in the 
                       second parameter, and replaces it with the duplicated
                       one. In this case after the call STDOUT_FILENO and
                       outfds[1] are copies of each other so we need to close
                       the original. 
                       
                       The macro checks for an EINTR error and repeats the
                       operation if interrupted. It's a standard macro. */
                    TEMP_FAILURE_RETRY(ret = dup2(outfds[1], STDOUT_FILENO));
                    if (ret == -1)
                    {
                        /* Debug is on stderr. */
                        JSE_ERROR("pipe() failed: %s", strerror(errno))

                        /* We are the child, so exit. */
                        exit(EXIT_FAILURE);
                    }
                }

                if (pErrfd != NULL)
                {
                    TEMP_FAILURE_RETRY(ret = dup2(errfds[1], STDERR_FILENO));
                    if (ret == -1)
                    {
                        /* This debug may not appear! */
                        JSE_ERROR("pipe() failed: %s", strerror(errno))

                        /* We are the child, so exit. */
                        exit(EXIT_FAILURE);
                    }
                }

                /* Close all read and write file descriptors as we have
                   duplicated the write ones to STDOUT_FILENO and to
                   STDERR_FILENO. And because we don't need the read ones
                   in the child. */

                if (pOutfd != NULL)
                {
                    (void) close(outfds[1]);
                    (void) close(outfds[0]);
                }

                if (pErrfd != NULL)
                {
                    (void) close(errfds[1]);
                    (void) close(errfds[0]);
                }

                /* Start the process. This does not return on success. */
                ret = execve(filename, args, env);

                /* This will appear on the child's stderr */
                JSE_DEBUG("%s: execve() failed: %s", filename, strerror(errno))

                /* Quit the child */
                exit(EXIT_FAILURE);
            }
            else if (pid != -1) /* The parent */
            {
                /* Close the write file descriptors in the parent as they
                   are used in the child. */
                (void) close(outfds[1]);
                (void) close(errfds[1]);

                /* Read from outfds[0] to read stdout from the child and
                   read from errfds[1] to read stderr from the child. */
                
                *pOutfd = outfds[0];
                *pErrfd = errfds[0];
            }
            else
            {
                /* Just in case close() changes it. */
                int _errno = errno;

                JSE_ERROR("fork() failed: %s", strerror(_errno))

                if (pOutfd != NULL)
                {
                    (void) close(outfds[0]);
                    (void) close(outfds[1]);
                }

                if (pErrfd != NULL)
                {
                    (void) close(errfds[0]);
                    (void) close(errfds[1]);
                }

                errno = _errno;
             }
        }
        else
        {
            /* Just in case close() changes it. */
            int _errno = errno;

            if (pOutfd != NULL)
            {
                (void) close(outfds[0]);
                (void) close(outfds[1]);
            }

            errno = _errno;
        }
    }

    JSE_VERBOSE("pid=%d", pid)
    return pid;
}

/**
 * Wait for a process with debug.
 *
 * This is basically waitpid but with lots of debug.
 *
 * @param pid the PID.
 * @param pwstatus the pointer to the process status
 * @param options the wait options
 *
 * @return the pid of the terminated task or -1 on error.
 */
static pid_t _waitpid(int pid, int * const pwstatus, int options)
{
    int wstatus = 0;

    pid_t ret = waitpid(pid, &wstatus, options);
    if (ret != -1)
    {
#ifdef JSE_DEBUG_ENABLED
        // ret == 0 if nothing exited.
        if (WIFEXITED(wstatus) && ret == pid)
        {
            JSE_DEBUG("Process terminated with exit status: %d", WEXITSTATUS(wstatus))
        }

        if (WIFSIGNALED(wstatus))
        {
            JSE_DEBUG("Process terminated by signal: %d", WTERMSIG(wstatus))
            if (WCOREDUMP(wstatus))
            {
                JSE_DEBUG("Process created a core dump!")
            }
        }

        if (WIFSTOPPED(wstatus))
        {
            JSE_DEBUG("Process stopped by signal: %d", WSTOPSIG(wstatus))
        }

        if (WIFCONTINUED(wstatus))
        {
            JSE_DEBUG("Process restarted!")
        }
#endif

        if (pwstatus != NULL)
        {
            *pwstatus = wstatus;
        }
    }
    else
    {
        JSE_ERROR("waitpid() failed: %s", strerror(errno))
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Reads from file descriptors until the close or the process exits.
 * 
 * This function waits on a select for WAIT_TIME microseconds and if
 * a file descriptor is ready, will read from that descriptor. Whether
 * or not the descriptor is ready it will also see if the specified
 * process has terminated. The function returns on either an error or
 * whether the process terminates.
 * 
 * If there is an error the values pointed to by pOutbuf, pOutlen, 
 * pErrbuf and pErrlen will be set to NULL and 0 and the buffers
 * freed.
 * 
 * @param pid the PID to wait for
 * @param outfd the stdout file descriptor.
 * @param errfs the stderr file descriptor.
 * @param pOutbuf a pointer to the stdout buffer.
 * @param pOutlen a pointer to the stdout length.
 * @param pErrbuf a pointer to the stderr buffer.
 * @param pErrlen a pointer to the stderr len.
 * @param pStatus a pointer to the exit status.
 * 
 * @return -1 on error or 0 on success.
 */
static int read_and_wait(int pid, int outfd, int errfd, 
    char ** const pOutbuf, size_t * const pOutlen, 
    char ** const pErrbuf, size_t * const pErrlen,
    int * pStatus)
{
    int ret = -1;

    off_t outoff = 0;
    off_t erroff = 0;
    ssize_t bytes;
    pid_t retpid;
    int wstatus;
    int sel;

    fd_set rfds;
    int nfd;

    /* Horrible polling loop but we need to see if the process has ended and if there's
       an data on the read file descriptors. */
    do {
        nfd = 0;
        FD_ZERO(&rfds);

        /* Only add open files to the file descriptor set. */

        if (outfd != -1)
        {
            FD_SET(outfd, &rfds);
            nfd = outfd + 1;
        }

        if (errfd != -1)
        {
            FD_SET(errfd, &rfds);
            nfd = max(errfd + 1, nfd);
        }

        /* Should we wait for something to read? */
        if (nfd != 0)
        {
            TEMP_FAILURE_RETRY(sel = select(nfd, &rfds, NULL, NULL, NULL));
            if (sel > 0)
            {
                if (outfd != -1 && FD_ISSET(outfd, &rfds))
                {
                    JSE_VERBOSE("outfd (%d) ready to read!", outfd);

                    /* The buffer is always bigger than the amount of data being
                    read so we have the offset in to the buffer and the size
                    of the buffer. This is to make memory handling efficient
                    by having powers of two scaling. */
                    bytes = jse_read_fd_once(outfd, pOutbuf, &outoff, pOutlen);
                    JSE_VERBOSE("bytes=%d", bytes)

                    if (bytes == -1)
                    {
                        JSE_ERROR("jse_read_fd_once() failed: %s", strerror(errno))
                        break;
                    }
                    else if (bytes == 0)
                    {
                        JSE_DEBUG("End of file!")
                        outfd = -1;
                    }
                }

                /* No else as could be both */
                if (errfd != -1 && FD_ISSET(errfd, &rfds))
                {
                    JSE_VERBOSE("errfd (%d) ready to read!", errfd);

                    bytes = jse_read_fd_once(errfd, pErrbuf, &erroff, pErrlen);
                    JSE_VERBOSE("bytes=%d", bytes)

                    if (bytes == -1)
                    {
                        JSE_ERROR("jse_read_fd_once() failed: %s", strerror(errno))
                        break;
                    }
                    else if (bytes == 0)
                    {
                        JSE_DEBUG("End of file!")
                        errfd = -1;
                    }
                }
            }
            else if (sel == -1)
            {
                JSE_ERROR("select() failed: %s", strerror(errno))
                break;
            }
            /* else sel == 0 which means we've timed out */
        }

        /* If end of file for both stdout and stderr can wait for pid.
           There's a race otherwise and you don't get the end of file
           for one or more of the streams. */
        if (outfd == -1 && errfd == -1)
        {
            retpid = _waitpid(pid, &wstatus, 0);
            if (retpid == -1)
            {
                JSE_ERROR("_waitpid() failed: %s", strerror(errno))
                break;
            }
            else
            {
                JSE_VERBOSE("Process terminated!")

                *pStatus = WEXITSTATUS(wstatus);
                ret = 0;
            }
        }

    /* Errors break out of the loop */
    } while (ret != 0);

    if (ret != 0)
    {
        /* Per C99 free(NULL) is a NOP. */
        free(*pOutbuf);
        free(*pErrbuf);

        *pOutbuf = NULL;
        *pOutlen = 0;
        *pErrbuf = NULL;
        *pErrlen = 0;
    }

    if (outfd != -1)
    {
        close(outfd);
    }

    if (errfd != -1)
    {
        close(errfd);
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

static pid_t fork_exec_read_and_wait(
    const char * const filename, char * const args[], char * const env[], 
    char ** const pOutbuf, size_t * const pOutlen, 
    char ** const pErrbuf, size_t * const pErrlen,
    int * pStatus)
{
    pid_t pid = -1;
    int outfd = -1;
    int errfd = -1;
    char * outbuf = NULL;
    size_t outlen = 4096;
    char * errbuf = NULL;
    size_t errlen = 4096;
    int status = 0;

    outbuf = (char *)calloc(sizeof(char), outlen);
    if (outbuf == NULL)
    {
        JSE_ERROR("calloc() failed: %s", strerror(errno))
    }
    else
    {
        errbuf = (char *)calloc(sizeof(char), errlen);
        if (errbuf == NULL)
        {
            JSE_ERROR("calloc() failed: %s", strerror(errno))
        }
        else
        {
            pid = fork_and_exec(filename, args, env, &outfd, &errfd);
            if (pid == -1)
            {
                JSE_ERROR("fork_and_exec() failed: %s", strerror(errno))
            }
            else
            {
                JSE_VERBOSE("outfd=%d, errfd=%d", outfd, errfd)

                if (read_and_wait(pid, outfd, errfd, &outbuf, &outlen, &errbuf, &errlen, &status) != 0)
                {
                    JSE_ERROR("read_and_wait() failed: %s", strerror(errno))
                    pid = -1;
                }
                else
                {
                    *pOutbuf = outbuf;
                    *pOutlen = outlen;
                    *pErrbuf = errbuf;
                    *pErrlen = errlen;
                    *pStatus = status;
                }
            }
        }
    }

    if (pid == -1)
    {
        /* Per C99 free(NULL) is a NOP */
        free(outbuf);
        free(errbuf);
    }

    JSE_VERBOSE("pid=%d", pid)
    return pid;
}

/**
 * Adds the value on the bottom of the stack to an vector of char* pointers
 *
 * This is a utility function. It pops the value off of the top of the
 * stack and strdups() the string adding the pointer to the string in to
 * the vector of char* pointers. The array is resized if necessary.
 *
 * The end of the array is terminated by a NULL pointer.
 *
 * @param pArray a pointer to the array of char* pointers.
 * @param pLength a pointer to the length of the array.
 * @param pSize a pointer to the size of the array buffer.
 *
 * @return -1 on error or 0 on success.
 */
static int add_string_to_string_array(const char * const str, char *** const pArray, size_t * const pLength, size_t * const pSize)
{
    int ret = -1;
    char ** array = *pArray;
    char * dup = strdup(str);
    size_t length = *pLength;
    size_t size = *pSize;

    JSE_ASSERT(array != NULL)
    JSE_ASSERT(size != 0)

    JSE_VERBOSE("Adding: %s", str)
    if (dup != NULL)
    {
        /* Then length does not include the terminating NULL */
        if (length + 2 >= size)
        {
            char ** newarray = NULL;

            /* Double the storage size, ensuring it is at least big enough */
            while (length + 2 >= size)
            {
                size *= 2;
            }

            newarray = (char **)realloc(array, size * sizeof(char*));
            if (newarray == NULL)
            {
                JSE_ERROR("realloc() failed: %s", strerror(errno))
                free(dup);
            }
            else
            {
                array = newarray;
            }
        }

        if (array != NULL)
        {
            array[length] = dup;
            length ++;
            array[length] = NULL;
            ret = 0;
        }
    }
    else
    {
        JSE_ERROR("strdup() failed: %s", strerror(errno))
    }

    *pArray = array;
    *pLength = length;
    *pSize = size;

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Converts a duktape array to an vector of pointers to C strings
 *
 * This function takes a duktape array at the specified index on the stack
 * and returns a vector of pointers to C strings. The vector is terminated
 * by a NULL pointer. The strings are copied from the duktape array. The
 * array should be freed by free_string_array().
 *
 * The first parameter is a pointer to a string (also copied) which is
 * added as the first element of the vector. This is optional and may be
 * NULL. It is there to support setting the first element of the args array
 * to be the filename of the process.
 *
 * @param ctx the duktape contextfree_string_array
 * @param obj_idx the stack index of the array
 * @param pArray a pointer to return the array pointer
 * @param first an optional first element of the array.
 *
 * @return 0 on success or -1 on error.
 */
static int duk_array_to_string_array(duk_context * ctx, duk_idx_t obj_idx, char *** const pArray, const char * first)
{
    int ret = -1;
    size_t size = 16;
    size_t length = 0;
    char ** array;

    array = (char **)calloc(sizeof(char*), size);
    if (array == NULL)
    {
        JSE_ERROR("calloc() failed: %s", strerror(errno))
    }
    else
    {
        ret = 0;
        if (first != NULL)
        {
            ret = add_string_to_string_array(first, &array, &length, &size);
            if (ret != 0)
            {
                JSE_ERROR("add_string_to_string_array() failed: %s", strerror(errno))
            }
        }

        if (ret == 0)
        {
            /* [ .... array ] */

            duk_size_t len = duk_get_length(ctx, obj_idx);
            duk_size_t idx;

            for (idx = 0; idx < len; idx ++)
            {
                duk_get_prop_index(ctx, obj_idx, idx);
                /* [ .... array ... , element ] */

                ret = add_string_to_string_array(duk_safe_to_string(ctx, -1), &array, &length, &size);
                duk_pop(ctx);
                /* [ .... array ... ] */

                if (ret != 0)
                {
                    JSE_ERROR("add_string_to_string_array() failed: %s", strerror(errno))
                    break;
                }
            }
        }
    }

    *pArray = array;

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Converts a duktape object to an vector of pointers to C strings
 *
 * This function takes a duktape object at the specified index on the stack
 * and returns a vector of pointers to C strings. The vector is terminated
 * by a NULL pointer. The strings are copied from the duktape array. The
 * array should be freed by free_string_array().
 *
 * The strings comprise "property=value" for each property and value pair and
 * is suitable for the env array passed to execve().
 *
 * @param ctx the duktape contextfree_string_array
 * @param obj_idx the stack index of the array
 * @param pArray a pointer to return the array pointer
 *
 * @return 0 on success or -1 on error.
 */
static int duk_object_to_string_array(duk_context * ctx, duk_idx_t obj_idx, char *** const pArray)
{
    int ret = -1;
    size_t size = 16;
    size_t length = 0;
    char ** array;

    array = (char **)calloc(sizeof(char*), size);
    if (array == NULL)
    {
        JSE_ERROR("calloc() failed: %s", strerror(errno))
    }
    else
    {
        /* [ .... object ] */

        /* Pushes enumerator on to the stack */
        duk_enum(ctx, obj_idx, 0);
        /* [ .... object ..., enumerator ] */

        ret = 0;
        while (duk_next(ctx, -1, true))
        {
            /* [ .... object ..., enumerator, key, value ] */
            duk_push_string(ctx, "=");
            /* [ .... object ..., enumerator, key, value, "=" ] */
            duk_swap(ctx, -1, -2);
            /* [ .... object ..., enumerator, key, "=", value ] */
            duk_concat(ctx, 3);
            /* [ .... object ..., enumerator, key + "=" + value ] */

            ret = add_string_to_string_array(duk_safe_to_string(ctx, -1), &array, &length, &size);
            duk_pop(ctx);
            /* [ .... object ..., enumerator ] */

            if (ret != 0)
            {
                JSE_ERROR("add_string_to_string_array() failed: %s", strerror(errno))
                break;
            }
        }

        /* Pop the enumerator */
        duk_pop(ctx);
        /* [ .... object ... ] */
    }

    *pArray = array;

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Frees the a string array vector.
 *
 * This function frees the string array vectors created by
 * duk_array_to_string_array() and duk_object_to_string_array(),
 * freeing both the strings and the array itself.
 *
 * @param array the array.
 */
static void free_string_array(char * array[])
{
    int idx;

    if (array != NULL)
    {
        for (idx = 0; array[idx] != NULL; idx++)
        {
            free(array[idx]);
        }

        free(array);
    }
}

/**
 * The binding for execProcess()
 *
 * This function spawns a child process and tracks it. It takes three
 * arguments, the process to execute, an optional array of arguments
 * and an optional environment object.
 *
 * It returns an object comprising the pid of the process, the exit
 * status, the stdout stream as a string, and the stderr stream as a
 * string.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
duk_ret_t do_exec_process(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    duk_idx_t num = duk_get_top(ctx);

    if (num == 0)
    {
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Insufficient arguments!");
    }
    else
    {
        const char * filename = NULL;
        char ** args = { NULL };
        char ** env = { NULL };
        char * outstr = NULL;
        size_t outlen = 0;
        char * errstr = NULL;
        size_t errlen = 0;
        pid_t pid = -1;
        int status = 0;
        duk_idx_t obj_idx;

        /* duktape indices are either 0, 1, 2, 3 from the top or
           -1, -2, -3 from the bottom */
        if (!duk_is_string(ctx, 0))
        {
            /* Does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Not a string!");
        }

        filename = duk_safe_to_string(ctx, 0);

        if (num > 1)
        {
            if (!duk_is_array(ctx, 1))
            {
                /* Does not return */
                JSE_THROW_TYPE_ERROR(ctx, "Not an array!");
            }
        }
        else
        {
            JSE_VERBOSE("No args array provided, creating one!")

            /* Put an empty array on to the stack  */
            duk_push_array(ctx);
            num ++;
        }

        if (num > 2)
        {
            if (!duk_is_object(ctx, 2))
            {
                /* Does not return */
                JSE_THROW_TYPE_ERROR(ctx, "Not an object!");
            }
        }
        else
        {
            JSE_VERBOSE("No env array provided, creating one!")

            /* Put an empty object on the stack */
            duk_push_object(ctx);
            num ++;
        }

        if (duk_array_to_string_array(ctx, 1, &args, filename) != 0)
        {
            /* Does not return */
            JSE_THROW_POSIX_ERROR(ctx, errno, "duk_array_to_string_array() failed: %s", strerror(errno));
        }

        if (duk_object_to_string_array(ctx, 2, &env) != 0)
        {
            free_string_array(args);

            /* Does not return */
            JSE_THROW_POSIX_ERROR(ctx, errno, "duk_object_to_string_array() failed: %s", strerror(errno));
        }

        /* DO THE THING */
        pid = fork_exec_read_and_wait(filename, args, env, &outstr, &outlen, &errstr, &errlen, &status);
        if (pid == -1)
        {
            free_string_array(env);
            free_string_array(args);

            /* Does not return */
            JSE_THROW_POSIX_ERROR(ctx, errno, "fork_exec_read_and_wait() failed: %s", strerror(errno));
        }

        JSE_VERBOSE("outstr=%p,outlen=%u,errstr=%p,errlen=%u",outstr,outlen,errstr,errlen)

        /* Create an object with pid, status, stdout and stderr as properties
           and leave it on the stack to be returned. */
        obj_idx = duk_push_object(ctx);
        /* [ .... object ] */
        duk_push_int(ctx, pid);
        /* [ .... object, pid ] */
        duk_put_prop_string(ctx, obj_idx, "pid");
        /* [ .... object ]  ('pid' = pid) */
        duk_push_int(ctx, status);
        /* [ .... object, status ] */
        duk_put_prop_string(ctx, obj_idx, "status");
        /* [ .... object ]  ('status' = status) */
        duk_push_string(ctx, outstr);
        /* [ .... object, outstr ] */
        duk_put_prop_string(ctx, obj_idx, "stdout");
        /* [ .... object ]  ('stdout' = outstr) */
        duk_push_string(ctx, errstr);
        /* [ .... object, errstr ] */
        duk_put_prop_string(ctx, obj_idx, "stderr");
        /* [ .... object ]  ('stderr' = errstr) */

        /* It is valid to do free(NULL) */
        free(errstr);
        free(outstr);

        /* It is valid to do free_string_array(NULL) */
        free_string_array(env);
        free_string_array(args);

        /* [ .... object ] */
        ret = 1;
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * The binding for sendSignal()
 * 
 * This function sends a signal to a process. It takes two arguments,
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

    JSE_VERBOSE("ret=%d", ret)
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

                duk_push_c_function(jse_ctx->ctx, do_exec_process, DUK_VARARGS);
                duk_put_global_string(jse_ctx->ctx, "execProcess");
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
