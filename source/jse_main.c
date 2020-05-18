
#ifdef ENABLE_FASTCGI
#include "fcgi_stdio.h"
#else
#include <stdio.h>
#endif
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <qdecoder.h>
#include <duktape.h>

#include "jse_debug.h"
#include "jse_common.h"
#include "jse_jscommon.h"

#include "jse_xml.h"

#ifdef BUILD_RDK
extern duk_int_t ccsp_cosa_module_open(duk_context *ctx);
#endif

#define MAX_SCRIPT_SIZE JSE_MAX_FILE_SIZE
#define EXIT_FATAL 3

#define JSE_REQUEST_OBJECT_NAME "Request"

/* List of HTTP status codes */
#define HTTP_STATUS_OK                     200
#define HTTP_STATUS_CREATED                201
#define HTTP_STATUS_ACCEPTED               202
#define HTTP_STATUS_NO_CONTENT             204
#define HTTP_STATUS_BAD_REQUEST            400
#define HTTP_STATUS_UNAUTHORIZED           401
#define HTTP_STATUS_FORBIDDEN              403
#define HTTP_STATUS_NOT_FOUND              404
#define HTTP_STATUS_METHOD_NOT_ALLOWED     405
#define HTTP_STATUS_IM_A_TEAPOT            418
#define HTTP_STATUS_INTERNAL_SERVER_ERROR  500
#define HTTP_STATUS_NOT_IMPLEMENTED        501

/* The requests to process */
static bool process_get     = false;
static bool process_post    = false;
static bool process_cookie  = false;

/* Flag to indicate it is an http request */
static bool is_http_request = false;

/* Linked list of 'headers' */
struct header_item_s
{
    char * name;
    char * value;
    struct header_item_s * next;
};

typedef struct header_item_s header_item_t;

static header_item_t *first_header_item = NULL;

/* Linked list of 'printed' items */
struct print_buffer_item_s
{
    const char * string;
    struct print_buffer_item_s * next;
};

typedef struct print_buffer_item_s print_buffer_item_t;

static print_buffer_item_t *first_print_item = NULL;
static print_buffer_item_t *last_print_item = NULL;

/* The HTTP status */
static int http_status = 0;

/* The content type */
static char* http_contenttype = NULL;

/* Cookie data */

struct cookie_data_s
{
    char * name;
    char * value;
    int expire_secs;
    char * path;
    char * domain;
    bool secure;
};

typedef struct cookie_data_s cookie_data_t;

static cookie_data_t http_cookie;

/**
 * Sets the value of a cookie.
 * 
 * @param cookie the cookie.
 * @param name the name.
 * @param value the value.
 * @param expire_secs the expiry time in seconds.
 * @param path the path (may be NULL).
 * @param domain the domain (may be NULL).
 * @param secure set true if a secure cookie.
 */
static void cookie_set(cookie_data_t * cookie, char * name, char * value, 
    int expire_secs, char * path, char * domain, bool secure)
{
     cookie->name = name;
     cookie->value = value;
     cookie->expire_secs = expire_secs;
     cookie->path = path;
     cookie->domain = domain;
     cookie->secure = secure;
}

/**
 * Destroys a cookie freeing the memory.
 * 
 * @param cookie a pointer to the cookie
 */
static void cookie_destroy(cookie_data_t * cookie)
{
    if (cookie->name != NULL)
    {
        free(cookie->name);
    }
 
    if (cookie->value != NULL)
    {
        free(cookie->value);
    }

    if (cookie->path != NULL)
    {
        free(cookie->path);
    }

    if (cookie->domain != NULL)
    {
        free(cookie->domain);
    }
}

/**
 * Tests for legal header names.
 * 
 * @param name the header name.
 * @return true if legal
 */
static bool header_validate(char * name)
{
    bool validated = true;

    if (!strcasecmp("status", name))
    {
        validated = false;
    }
    else
    if (!strcasecmp("set-cookie", name))
    {
        validated = false;
    }
    else
    if (!strcasecmp("content-type", name))
    {
        validated = false;
    }

    return validated;
}

/**
 * Sets a header in the header list.
 *
 * @param name the header name.
 * @param value the header value.
 * @return an error code or 0 on success.
 */
static int header_set(char * name, char * value)
{
    int ret = -1;
    header_item_t * item = first_header_item;
    header_item_t * prev = NULL;

    /* Iterate through the headers looking to see if it is already defined. */
    while (item != NULL) 
    {
        if (!strcasecmp(item->name, name)) 
        {
            free(item->name);
            free(item->value);

            item->name = name;
            item->value = value;
            ret = 0;
            break;
        }

        prev = item;
        item = item->next;
    }

    /* Didn't find the header */
    if (item == NULL)
    {
        item = (header_item_t *)calloc(sizeof(header_item_t), 1);
        if (item != NULL)
        {
            item->name = name;
            item->value = value;

            if (first_header_item == NULL)
            {
                first_header_item = item;
            }
            else
            {
                prev->next = item;
            }

            ret = 0;    
        }
        else
        {
            JSE_DEBUG("calloc() failed: %s", strerror(errno));
        }
    }

    return ret;
}

/**
 * Returns an appropriate HTTP status message for a status code.
 * 
 * @param status the status code.
 * @return the message.
 */
static char* msg_for_http_status(int status)
{
    char * msg = NULL;

    switch (status)
    {
        case HTTP_STATUS_ACCEPTED:
            msg = "Accepted";
            break;
        case HTTP_STATUS_BAD_REQUEST:
            msg = "Bad Request";
            break;
        case HTTP_STATUS_CREATED:
            msg = "Created";
            break;
        case HTTP_STATUS_FORBIDDEN:
            msg = "Forbidden";
            break;
        case HTTP_STATUS_IM_A_TEAPOT:
            msg = "I'm a teapot";
            break;
        case HTTP_STATUS_INTERNAL_SERVER_ERROR:
            msg = "Internal Server Error";
            break;
        case HTTP_STATUS_METHOD_NOT_ALLOWED:
            msg = "Method Not Allowed";
            break;
        case HTTP_STATUS_NO_CONTENT:
            msg = "No Content";
            break;
        case HTTP_STATUS_NOT_FOUND:
            msg = "Not Found";
            break;
        case HTTP_STATUS_NOT_IMPLEMENTED:
            msg = "Not Implemented";
            break;
        case HTTP_STATUS_OK:
            msg = "OK";
            break;
        case HTTP_STATUS_UNAUTHORIZED:
            msg = "Unauthorized";
            break;
        default:
            JSE_WARNING("Unrecognised status code: %d", status)
            msg = "Unrecognised Status Code";
            break;
    }

    return msg;
}

/**
 * Returns the status line to the server.
 * 
 * @param status the HTTP status.
 */
static void return_http_status(int status)
{
    printf("Status: %d %s\r\n", status, msg_for_http_status(status));
}

/**
 * Returns an error to the server.
 *
 * @param jse_ctx the jse context.
 * @param status the HTTP status.
 * @param mimetype the content mimetype.
 * @param format a printf style formatter.
 */
static void return_error(jse_context_t *jse_ctx, int status, const char* mimetype, const char *format, ...)
{
    char buffer[1024];
    int len;
    va_list ap;

    va_start(ap, format);
    len = vsnprintf(buffer, sizeof(buffer), format, ap);
    if (len >= (int)sizeof(buffer))
    {
        strcpy(buffer, 
            "<html><head><title>Internal server error</title></head><body>Buffer overflow in the error handler!</body></html>\r\n");
        status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }
    va_end(ap);

    JSE_ERROR(buffer);

    /* Setting the content type terminates the header so output the status now */
    return_http_status(status);

    qcgires_setcontenttype(jse_ctx->req, mimetype);
    printf("%s\r\n", buffer);
}

/**
 * Sets any cookies before creating the HTTP header.
 *
 * @param jse_ctx the jse context.
 */
static void set_any_cookies(jse_context_t *jse_ctx)
{
    qentry_t *req = jse_ctx->req;

    if (http_cookie.name != NULL)
    {
        if (!qcgires_setcookie(req, http_cookie.name, http_cookie.value, 
            http_cookie.expire_secs, http_cookie.path, http_cookie.domain, http_cookie.secure))
        {
            JSE_ERROR("Failed to set cookie!")
        }

        cookie_destroy(&http_cookie);
    }    
}

/**
 * Returns a response to the server.
 *
 * @param status the HTTP status.
 * @param contenttype the content type.
 */
static void return_response(jse_context_t *jse_ctx, int status, const char* contenttype)
{
    return_http_status(status);

    /* Iterate through the headers */
    while (first_header_item != NULL)
    {
        header_item_t * item = first_header_item;

        printf("%s: %s\r\n", item->name, item->value);
        first_header_item = item->next;

        free(item->value);
        free(item->name);
        free(item);
    }

    set_any_cookies(jse_ctx);

    /* This basically ends the header so has to be done last. */
    qcgires_setcontenttype(jse_ctx->req, contenttype);

    /* Iterate through the buffer of printed content. */
    while (first_print_item != NULL)
    {
        print_buffer_item_t * item = first_print_item;

        printf("%s", item->string);
        first_print_item = item->next;
        
        free(item);
    }
    last_print_item = NULL;
}

/**
 * Handle fatal duktape errors.
 *
 * @param userdata a pointer to the jse context as a void*.
 * @param msg the error message.
 */
static void handle_fatal_error(void* userdata, const char *msg)
{
    jse_context_t *jse_ctx = (jse_context_t*)userdata;

    JSE_ERROR(msg)

    /* Only output HTTP data if we have a qdecoder request. */
    if (jse_ctx != NULL && jse_ctx->req != NULL)
    {
        return_error(jse_ctx, HTTP_STATUS_INTERNAL_SERVER_ERROR, "text/html", 
            "<html><head><title>Internal server error</title></head><body>%s</body></html>", msg);

        jse_ctx->req->free(jse_ctx->req);
        jse_ctx->req = NULL;
    }

    if (jse_ctx != NULL)
    {
        free(jse_ctx);
    }

    exit(EXIT_FATAL);
}

/**
 * Initialise duktape
 *
 * @param jse_ctx the jse context.
 *
 * @return the duktape context.
 */
static duk_context *init_duktape(jse_context_t *jse_ctx)
{
    JSE_VERBOSE("init_duktape()")

    /* TODO: low mem allocators */
    jse_ctx->ctx = duk_create_heap(NULL, NULL, NULL, jse_ctx, handle_fatal_error);

    return jse_ctx->ctx;
}

/**
 * Cleanup duktape
 *
 * @param jse_ctx the jse context.
 */
static void cleanup_duktape(jse_context_t *jse_ctx)
{
    JSE_VERBOSE("cleanup_duktape()")

    duk_destroy_heap(jse_ctx->ctx);
}

/**
 * Runs JavaScript code provided via stdin.
 *
 * In case of an Error, an Error object remains on the stack when the
 * function exits.
 * 
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
static duk_int_t run_stdin(jse_context_t *jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;
    char *buffer = NULL;
    size_t buffer_size = 0;
    size_t size = 0;
    ssize_t bytes = 0;

    JSE_VERBOSE("run_stdin()")

    buffer_size = 4096;
    buffer = (char*)malloc(buffer_size);
    if (buffer == NULL)
    {
        goto error;
    }

    do
    {
        /* Size is size of contents */
        TEMP_FAILURE_RETRY(bytes = read(STDIN_FILENO, buffer + size, buffer_size - size));
        if (bytes == -1)
        {
            goto error;
        }

        if (bytes > 0)
        {
            size += bytes;

            /* Do we need to resize the buffer */
            if (size + bytes == buffer_size)
            {
                /* Can't expand it any more. */
                if (buffer_size == MAX_SCRIPT_SIZE)
                {
                    goto error;
                }

                buffer_size *= 2;

                if (buffer_size > MAX_SCRIPT_SIZE)
                {
                    buffer_size = MAX_SCRIPT_SIZE;
                }

                buffer = realloc(buffer, buffer_size);
                if (buffer == NULL)
                {
                    goto error;
                }
            }
        }
    }
    while (bytes > 0);

    /* trim off excess memory. */
    buffer = realloc(buffer, size + 1);
    if (buffer == NULL)
    {
        goto error;
    }

    *(buffer + size) = '\0';

    if (jse_run_buffer(jse_ctx, buffer, size) == DUK_EXEC_SUCCESS)
    {
        ret = 0;
    }

    free(buffer);

    JSE_VERBOSE("ret=%d", ret)
    return ret;
        
error:
    JSE_ERROR("Error: %s", strerror(errno))
    duk_push_error_object(jse_ctx->ctx, DUK_ERR_ERROR, "%s", strerror(errno));

    if (buffer != NULL)
    {
        free(buffer);
    }

    JSE_VERBOSE("ret=DUK_RET_ERROR")
    return DUK_RET_ERROR;
}

/**
 * Runs JavaScript code provided in a file.
 *
 * In case of an Error, an Error object remains on the stack when the
 * function exits.
 * 
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
static duk_int_t run_file(jse_context_t *jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;
    char *buffer = NULL;
    size_t size = 0;

    JSE_VERBOSE("run_file()")

    if (jse_read_file(jse_ctx->filename, &buffer, &size) > 0)
    {
        if (jse_run_buffer(jse_ctx, buffer, size) == DUK_EXEC_SUCCESS)
        {
            ret = 0;
        }

        free(buffer);
    }
    else
    {
        JSE_ERROR("Error: %s: %s", jse_ctx->filename, strerror(errno))
        duk_push_error_object(jse_ctx->ctx, DUK_ERR_ERROR, 
            "%s: %s", jse_ctx->filename, strerror(errno));
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Prints a string to the output.
 *
 * This function prints a string to the output. Typically this is standard
 * out. When handling HTTP requests output has to be delayed until the
 * HTTP header is output so all output is buffered internally and output
 * once the header has been generated.
 *
 * @param ctx the duktape context.
 * @return the number of values on the stack or -1 for an error.
 */
static duk_ret_t do_print(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    JSE_ASSERT(ctx != NULL)

    if (is_http_request)
    {
        /* Need to delay output because of errors, headers, etc */
        char * string = strdup(duk_safe_to_string(ctx, -1));
        if (string != NULL)
        {
            print_buffer_item_t *item = (print_buffer_item_t*)calloc(sizeof(print_buffer_item_t), 1);
            if (item != NULL)
            {
                item->string = string;
                if (last_print_item != NULL)
                {
                    last_print_item->next = item;
                }

                last_print_item = item;

                if (first_print_item == NULL)
                {
                    first_print_item = item;
                }

                /* Return undefined */
                ret = 0;
            }
            else 
            {
                JSE_ERROR("calloc() failed: %s", strerror(errno))
                free(string);
            }
        }
        else 
        {
            JSE_ERROR("strdup() failed: %s", strerror(errno))
        }
    }
    else
    {
        printf("%s", duk_safe_to_string(ctx, -1));

        /* Return undefined */
        ret = 0;
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Sets the HTTP status.
 *
 * This function sets the HTTP status. It expects one integer on the duktape
 * stack that specifies the status value.
 *
 * @param ctx the duktape context.
 * @return the number of values on the stack or -1 for an error.
 */
static duk_ret_t do_setHTTPStatus(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    if (duk_is_number(ctx, -1))
    {
        http_status = (int)duk_get_int_default(ctx, -1, HTTP_STATUS_OK);

        JSE_DEBUG("status = %d", http_status)

        /* Return undefined */
        ret = 0;
    }
    else
    {
        JSE_ERROR("Invalid argument!")
        ret = DUK_RET_TYPE_ERROR;
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Sets the mimetype.
 *
 * This function sets the content type. It expects one string on the duktape
 * stack that specifies the content type.
 *
 * @param ctx the duktape context.
 * @return the number of values on the stack or -1 for an error.
 */
static duk_ret_t do_setContentType(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    if (duk_is_string(ctx, -1))
    {
        char * string = strdup(duk_safe_to_string(ctx, -1));
        if (string != NULL)
        {
            if (http_contenttype != NULL)
            {
                free(http_contenttype);
            }

            http_contenttype = string;

            JSE_DEBUG("mimetype = %s", http_contenttype);

            /* Return undefined */
            ret = 0;
        }
        else 
        {
            JSE_ERROR("strdup() failed: %s", strerror(errno))
        }
    }
    else
    {
        JSE_ERROR("Invalid argument \"mimetype\"!")
        ret = DUK_RET_TYPE_ERROR;
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Sets the cookie value.
 *
 * This function sets the cookie value. It expects five arguments, the
 * cookie name, the cookie value, an expiry time in seconds (0 expire
 * now), an optional path (may be null), an optional domain (may be
 * null), and a boolean indicating a secure cookie or not.
 *
 * @param ctx the duktape context.
 * @return the number of values on the stack or -1 for an error.
 */
static duk_ret_t do_setCookie(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    char * name = NULL;
    char * value = NULL;
    int expire_secs = 0;
    char * path = NULL;
    char * domain = NULL;
    bool secure = false;

    if (duk_is_string(ctx, -6))
    {
        name = strdup(duk_safe_to_string(ctx, -6));
        if (name == NULL)
        {
            JSE_ERROR("strdup() failed: %s", strerror(errno))
            goto error;
        }
    }
    else
    {
        JSE_ERROR("Invalid argument \"name\" (%d)", duk_get_type(ctx, -6))
        ret = DUK_RET_TYPE_ERROR;
        goto error;
    }

    if (duk_is_string(ctx, -5))
    {
        value = strdup(duk_safe_to_string(ctx, -5));
        if (value == NULL)
        {
            JSE_ERROR("strdup() failed: %s", strerror(errno))
            goto error;
        }
    }
    else
    {
        JSE_ERROR("Invalid argument \"value\" (%d)", duk_get_type(ctx, -5))
        ret = DUK_RET_TYPE_ERROR;
        goto error;
    }

    if (duk_is_number(ctx, -4))
    {
        expire_secs = (int)duk_get_int_default(ctx, -4, 0);
    }
    else
    {
        JSE_ERROR("Invalid argument \"expire_secs\" (%d)", duk_get_type(ctx, -4))
        ret = DUK_RET_TYPE_ERROR;
        goto error;
    }

    if (!duk_is_null(ctx, -3))
    {
        if (duk_is_string(ctx, -3))
        {
            path = strdup(duk_safe_to_string(ctx, -3));
            if (path == NULL)
            {
                JSE_ERROR("strdup() failed: %s", strerror(errno))
                goto error;
            }
        }
        else
        {
            JSE_ERROR("Invalid argument \"path\" (%d)", duk_get_type(ctx, -3))
            goto error;
        }
    }

    if (!duk_is_null(ctx, -2))
    {
        if (duk_is_string(ctx, -2))
        {
            domain = strdup(duk_safe_to_string(ctx, -2));
            if (domain == NULL)
            {
                JSE_ERROR("strdup() failed: %s", strerror(errno))
                goto error;
            }
        }
        else
        {
            JSE_ERROR("Invalid argument \"domain\" (%d)", duk_get_type(ctx, -2))
            goto error;
        }
    }

    if (duk_is_boolean(ctx, -1))
    {
        secure = (bool)duk_get_boolean_default(ctx, -1, false);
    }
    else
    {
        JSE_ERROR("Invalid argument \"secure\" (%d)", duk_get_type(ctx, -1))
        goto error;
    }

    cookie_destroy(&http_cookie);
    cookie_set(&http_cookie, name, value, expire_secs, path, domain, secure);

    JSE_DEBUG("cookie=\"%s\"=\"%s\",expires=%d,path=\"%s\",domain=\"%s\",secure=%s",
        http_cookie.name, http_cookie.value, http_cookie.expire_secs,
        http_cookie.path != NULL ? http_cookie.path : "(null)",
        http_cookie.domain !=NULL ? http_cookie.domain : "(null)",
        http_cookie.secure ? "true" : "false")

    JSE_VERBOSE("ret=0")
    return 0;

error:
    if (domain != NULL)
    {
        free(domain);
    }

    if (path != NULL)
    {
        free(path);
    }

    if (value != NULL)
    {
        free(value);
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Sets or adds an HTTP header
 * 
 * This function sets or adds a header. The function expects two strings
 * the first being the header name. The second being the value. If the
 * name already exists it is replaced by the new value. The test for the
 * name is case insensitive.
 *
 * @param ctx the duktape context.
 * @return the number of values on the stack or -1 for an error.
 */
static duk_ret_t do_setHeader(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    char * name = NULL;
    char * value = NULL;

    if (duk_is_string(ctx, -2))
    {
        name = strdup(duk_safe_to_string(ctx, -2));
        if (name != NULL)
        {
            if (header_validate(name))
            {
                if (!duk_is_null(ctx, -1))
                {
                    if (duk_is_string(ctx, -1))
                    {
                        value = strdup(duk_safe_to_string(ctx, -1));
                        if (value != NULL)
                        {
                            if (header_set(name, value) == 0)
                            {
                                ret = 0;
                            }
                            else
                            {
                                free(value);
                                free(name);
                            }
                        }
                        else
                        {
                            JSE_ERROR("strdup() failed: %s", strerror(errno));
                            free(name);
                        }
                    }
                    else
                    {
                        JSE_ERROR("Invalid argument: value (%d)", duk_get_type(ctx, -1))
                        free(name);

                        ret = DUK_RET_TYPE_ERROR;
                    }
                }
            }
            else
            {
                JSE_ERROR("Illegal header name: \"%s\"", name);
                free(name);

                ret = DUK_RET_TYPE_ERROR;
            }
        }
        else
        {
            JSE_ERROR("strdup() failed: %s", strerror(errno));
        }
    }
    else
    {
       JSE_ERROR("Invalid argument \"name\" (%d)", duk_get_type(ctx, -2))
       ret = DUK_RET_TYPE_ERROR;
    }

    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Binds external functions to the JavaScript engine.
 *
 * @param jse_ctx the jse context.
 * @return an error code or 0.
 */
static duk_int_t bind_functions(jse_context_t *jse_ctx)
{
    duk_int_t ret = 0;

    JSE_VERBOSE("bind_functions()")
    JSE_ASSERT(jse_ctx != NULL);

    /* Bind built ins */
    duk_push_c_function(jse_ctx->ctx, do_print, 1);
    duk_put_global_string(jse_ctx->ctx, "print");

    duk_push_c_function(jse_ctx->ctx, do_setHTTPStatus, 1);
    duk_put_global_string(jse_ctx->ctx, "setHTTPStatus");

    duk_push_c_function(jse_ctx->ctx, do_setContentType, 1);
    duk_put_global_string(jse_ctx->ctx, "setContentType");

    duk_push_c_function(jse_ctx->ctx, do_setCookie, 6);
    duk_put_global_string(jse_ctx->ctx, "setCookie");

    duk_push_c_function(jse_ctx->ctx, do_setHeader, 2);
    duk_put_global_string(jse_ctx->ctx, "setHeader");

    if ((ret = jse_bind_jscommon(jse_ctx)) != 0)
    {
        JSE_ERROR("Failed to bind jscommon functions!");
    }
#ifdef ENABLE_LIBXML2
    else if ((ret = jse_bind_xml(jse_ctx)) != 0)
    {
        JSE_ERROR("Failed to bind xml functions!");
    }
#endif
#ifdef BUILD_RDK
    else
    {
        if ((ret = ccsp_cosa_module_open(jse_ctx->ctx)) != 0)
        {
            JSE_ERROR("Failed to bind cosa functions!");
        }
        else
        {
            duk_put_global_string(jse_ctx->ctx, "Cosa");
        }
    }
#endif
    return ret;
}

/**
 * Utility method that adds an environment variable as an object property
 *
 * Given the object at idx, this function adds the environment variable
 * env, given it is defined, as a property called env with the value of
 * the environment variable env.
 *
 * @param ctx the duktape context.
 * @param idx the object index.
 * @param env the environment variable name.
 * @return an error code or 0.
 */
static duk_int_t add_env_as_object_property(duk_context *ctx, duk_idx_t idx, const char* env)
{
    const char * value = NULL;

    JSE_ASSERT(ctx != NULL)
    JSE_ASSERT(env != NULL)

    value = getenv(env);
    if (value != NULL)
    {
        duk_push_string(ctx, value);
        duk_put_prop_string(ctx, idx, env);
    }

    return 0;
}

/**
 * Creates a request object.
 *
 * The query string parameters are passed as properties in an object on
 * the duktape stack.
 *
 * @param jse_ctx the jse context.
 * @return an error code or 0.
 */
static duk_int_t create_request_object(jse_context_t *jse_ctx)
{
    duk_context *ctx = jse_ctx->ctx;
    qentry_t *req = jse_ctx->req;
    qentobj_t obj;
    duk_idx_t idx, idx2;

    /* Push an object on to the stack comprising all the parameters */
    memset(&obj, 0, sizeof(obj));
    idx = duk_push_object(ctx);

    idx2 = duk_push_object(ctx);
    while (req->getnext(req, &obj, NULL, false))
    {
        JSE_DEBUG("Parameter: name=\"%s\", value=\"%s\"", obj.name, (char*)obj.data)
        duk_push_string(ctx, (char*)obj.data);
        duk_put_prop_string(ctx, idx2, obj.name);
    }

    duk_put_prop_string(ctx, idx, "QueryParameters");

    add_env_as_object_property(ctx, idx, "DOCUMENT_ROOT");
    add_env_as_object_property(ctx, idx, "HTTP_COOKIE");
    add_env_as_object_property(ctx, idx, "HTTP_REFERER");
    add_env_as_object_property(ctx, idx, "QUERY_STRING");
    add_env_as_object_property(ctx, idx, "REQUEST_METHOD");
    add_env_as_object_property(ctx, idx, "REQUEST_URI");
    add_env_as_object_property(ctx, idx, "SCRIPT_FILENAME");
    add_env_as_object_property(ctx, idx, "SCRIPT_NAME");

    duk_put_global_string(ctx, JSE_REQUEST_OBJECT_NAME);

    return 0;
}

/**
 * Handle an HTTP request
 *
 * @param jse_ctx the jse context.
 * @return an error code or 0.
 */
static duk_int_t handle_request(jse_context_t *jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_VERBOSE("handle_request()")

    ret = bind_functions(jse_ctx); 
    if (ret == 0)
    {
        /* Parse the request */
        if (process_post)
        {
            jse_ctx->req = qcgireq_parse(jse_ctx->req, Q_CGI_POST);
        }

        if (process_get)
        {
            jse_ctx->req = qcgireq_parse(jse_ctx->req, Q_CGI_GET);
        }

        if (process_cookie)
        {
            jse_ctx->req = qcgireq_parse(jse_ctx->req, Q_CGI_COOKIE);
        }

        JSE_VERBOSE("jse_ctx->req=%p", jse_ctx->req)

        /* An HTTP method was set, we should parse as HTTP */
        if (jse_ctx->req != NULL)
        {
            is_http_request = true;

            ret = create_request_object(jse_ctx);
            if (ret == 0)
            {
                if (jse_ctx->filename != NULL)
                {
                    ret = run_file(jse_ctx);
                }
                else
                {
                    ret = run_stdin(jse_ctx);
                }
            }

            JSE_VERBOSE("ret=%d", ret)
            if (ret == 0)
            {
                return_response(jse_ctx, 
                     http_status != 0 ? http_status : HTTP_STATUS_OK, 
                     http_contenttype != NULL ? http_contenttype : "text/plain");
            }
            else
            {
                /* In case of an error, an error object is on the duktape stack */
                return_error(jse_ctx, HTTP_STATUS_INTERNAL_SERVER_ERROR, "text/html", 
                    "<html><head><title>Internal server error</title></head><body>%s</body></html>",
                    duk_safe_to_string(jse_ctx->ctx, -1));

                duk_pop(jse_ctx->ctx);
            }
        }
        /* An HTTP method was set but we failed to parse. */
        else if (process_post != 0 || process_get != 0 || process_cookie != 0)
        {
            jse_ctx->req = qcgireq_parse(jse_ctx->req, Q_CGI_GET);
            if (jse_ctx->req != NULL)
            {
                return_error(jse_ctx, HTTP_STATUS_METHOD_NOT_ALLOWED, "text/html", 
                    "<html><head><title>Method not allowed</title></head><body>Method not allowed</body></html>");
            }
            else
            {
                JSE_ERROR("Method not allowed!");
            }

            ret = DUK_RET_ERROR;
        }
        /* Treat as regular output */
        else
        {
            if (jse_ctx->filename != NULL)
            {
                ret = run_file(jse_ctx);
            }
            else
            {
                ret = run_stdin(jse_ctx);
            }

            JSE_VERBOSE("ret=%d", ret)
            if (ret != 0)
            {
                fprintf(stderr, "Script error: %s\n", duk_safe_to_string(jse_ctx->ctx, -1));
            }
        }

        /* clean up qdecoder */
        if (jse_ctx->req != NULL)
        {
            jse_ctx->req->free(jse_ctx->req);
            jse_ctx->req = NULL;
        }
    }

    return ret;
}

/**
 * Displays help on stderr.
 */
static void help(const char* name)
{
    fprintf(stderr,
"Usage: %s [OPTION]... [FILENAME]\n" \
"Run a JavaScript script as a CGI request.\n" \
"\n" \
"Mandatory arguments to long options are mandatory for short options too.\n" \
"  -c, --cookies            Handle cookie requests.\n" \
"  -g, --get                Handle GET requests.\n" \
"  -h, --help               Display this help.\n" \
"  -p, --post               Handle POST requests.\n" \
"  -v, --verbose            Verbosity. Multiple uses increases vebosity.\n" \
"\n" \
"Exit status:\n" \
" 0  if OK,\n" \
" 1  if an ERROR,\n"
" 2  if a FATAL error.\n", name);
}

/**
 * main!
 */
int main(int argc, char **argv)
{
    char * jseargs = NULL;
    char * filename = NULL;
    char * arg = NULL;
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_DEBUG_INIT()
    JSE_VERBOSE("main()")

    while (1)
    {
        int c, option_index = 0;
        static struct option long_options[] =
        {
            {"cookies",  no_argument,       0, 'c' },
            {"get",      no_argument,       0, 'g' },
            {"help",     no_argument,       0, 'h' },
            {"post",     no_argument,       0, 'p' },
            {"verbose",  no_argument,       0, 'v' },
            {0,          0,                 0,  0  }
        };

        c = getopt_long(argc, argv, "cghpv", long_options, &option_index);
        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'c':
                JSE_DEBUG("Cookie processing enabled!")
                process_cookie = true;
                break;

            case 'g':
                JSE_DEBUG("GET processing enabled!")
                process_get = true;
                break;

            case 'p':
                JSE_DEBUG("POST processing enabled!")
                process_post = true;
                break;

            case 'h':
                help(argv[0]);
                exit(EXIT_SUCCESS);

            case 'v':
                jse_verbosity ++;
                break;

            default:
                JSE_ERROR("Invalid option")
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        JSE_DEBUG("Filename: %s", argv[optind])
        filename = strdup(argv[optind++]);
        if (filename == NULL)
        {
            JSE_ERROR("strdup() failed: %s", strerror(errno))
            exit(EXIT_FAILURE);
        }

        if (optind < argc)
        {
            JSE_ERROR("Too many arguments!")

            help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    jseargs = getenv("JSE_ARGUMENTS");
    if (jseargs != NULL)
    {
        char * env = strdup(jseargs);
        if (env == NULL)
        {
            JSE_ERROR("strdup() failed: %s", strerror(errno))
            exit(EXIT_FAILURE);
        }

        arg = strtok(env, " ");
        while (arg != NULL)
        {
            JSE_DEBUG("arg=%s", arg)

            if (!strcmp("-c", arg) || !strcmp("--cookie", arg))
            {
                process_cookie = true;
            }
            else
            if (!strcmp("-g", arg) || !strcmp("--get", arg))
            {
                process_get = true;
            }
            else
            if (!strcmp("-p", arg) || !strcmp("--post", arg))
            {
                process_post = true;
            }
            else
            if (!strcmp("-v", arg) || !strcmp("--verbose", arg))
            {
                jse_verbosity ++;
            }

            arg = strtok(NULL, " ");
        }
    }

    JSE_VERBOSE("main loop...")
    jse_context_t *jse_ctx = jse_context_create(filename);
    if (jse_ctx != NULL)
    {
        init_duktape(jse_ctx);

#ifdef ENABLE_FASTCGI
        while(FCGI_Accept() >= 0) {
#endif

        ret = handle_request(jse_ctx);

#ifdef ENABLE_FASTCGI
        }
#endif

        cleanup_duktape(jse_ctx);
    }

    jse_context_destroy(jse_ctx);

    if (ret != 0)
    {
        return EXIT_FATAL;
    }

    return EXIT_SUCCESS;
}
