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

#ifdef ENABLE_FASTCGI
#include "fcgi_stdio.h"
#else
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
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
#include "jse_jserror.h"
#include "jse_jsprocess.h"

#ifdef ENABLE_LIBXML2
#include "jse_xml.h"
#endif

#ifdef BUILD_RDK
#include "jse_cosa.h"

/** Flag that is set once the Cosa API is initialised successfully */
static bool cosa_initialised = false;
#endif

#ifdef ENABLE_LIBCRYPTO
#include "jse_crypt.h"
#endif

#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(a)
#endif
#endif

#define CONST_STRLEN(a)  (sizeof(a) - 1)

#define MAX_SCRIPT_SIZE JSE_MAX_FILE_SIZE
#define EXIT_FATAL 3

#define JSE_REQUEST_OBJECT_NAME "Request"
#define JSE_UPLOAD_DIR "/var/jse/uploads"
#define JSE_UPLOAD_EXPIRY_SECS 3600

/* List of HTTP status codes */
#define HTTP_STATUS_OK                     200
#define HTTP_STATUS_CREATED                201
#define HTTP_STATUS_ACCEPTED               202
#define HTTP_STATUS_NO_CONTENT             204
#define HTTP_STATUS_MOVED_PERMANENTLY      301
#define HTTP_STATUS_FOUND                  302
#define HTTP_STATUS_BAD_REQUEST            400
#define HTTP_STATUS_UNAUTHORIZED           401
#define HTTP_STATUS_FORBIDDEN              403
#define HTTP_STATUS_NOT_FOUND              404
#define HTTP_STATUS_METHOD_NOT_ALLOWED     405
#define HTTP_STATUS_IM_A_TEAPOT            418
#define HTTP_STATUS_INTERNAL_SERVER_ERROR  500
#define HTTP_STATUS_NOT_IMPLEMENTED        501

/* The environment. See man environ */
extern char **environ;

/* Initial body read buffer size */
#define BODY_READ_BUF_SIZE 4096

/* The upload directory */
static char * upload_dir = NULL;

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
    const void * string;
    size_t length;
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

/* Linked list of 'file' objects */
struct file_object_s
{
    char* name;
    duk_int_t idx;
    struct file_object_s * next;
};

/**
 * @brief Sets the value of a cookie.
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
 * @brief Destroys a cookie freeing the memory.
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
 * @brief Tests for legal header names.
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
 * @brief Sets a header in the header list.
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
            JSE_DEBUG("calloc() failed: %s", strerror(errno))
        }
    }

    return ret;
}

/**
 * @brief Returns an appropriate HTTP status message for a status code.
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
        case HTTP_STATUS_MOVED_PERMANENTLY:
            msg = "Moved Permanently";
            break;
        case HTTP_STATUS_FOUND:
            msg = "Found";
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
 * @brief Returns the status line to the server.
 * 
 * @param status the HTTP status.
 */
static void return_http_status(int status)
{
    printf("Status: %d %s\r\n", status, msg_for_http_status(status));
}

/**
 * @brief Returns an error to the server.
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
            "<html>"
            "<head><title>Internal server error</title></head>"
            "<body>Buffer overflow in the error handler!</body>"
            "</html>\r\n");
        status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }
    va_end(ap);

    JSE_ERROR(buffer)

    /* Setting the content type terminates the header so output the status now */
    return_http_status(status);

    qcgires_setcontenttype(jse_ctx->req, mimetype);
    printf("%s\r\n", buffer);
}

/**
 * @brief Sets any cookies before creating the HTTP header.
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
 * @brief Returns a response to the server.
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

        // So we can handle strings with null characters.
        fwrite(item->string, item->length, 1, stdout);
        first_print_item = item->next;
        
        free(item);
    }
    last_print_item = NULL;
}

__attribute__((noreturn))
/**
 * @brief Handle fatal duktape errors.
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
 * @brief Initialise duktape
 *
 * @param jse_ctx the jse context.
 *
 * @return the duktape context.
 */
static duk_context *init_duktape(jse_context_t *jse_ctx)
{
    JSE_ENTER("init_duktape(%p)", jse_ctx)

    /* TODO: low mem allocators */
    jse_ctx->ctx = duk_create_heap(NULL, NULL, NULL, jse_ctx, handle_fatal_error);

    JSE_EXIT("init_duktape()=%p", jse_ctx->ctx)
    return jse_ctx->ctx;
}

/**
 * @brief Cleanup duktape
 *
 * @param jse_ctx the jse context.
 */
static void cleanup_duktape(jse_context_t *jse_ctx)
{
    JSE_ENTER("cleanup_duktape(%p)", jse_ctx)

    duk_destroy_heap(jse_ctx->ctx);

    JSE_EXIT("cleanup_duktape()")
}

/**
 * @brief Runs JavaScript code provided via stdin.
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
    void *buffer = NULL;
    size_t size = 0;

    JSE_ENTER("run_stdin(%p)", jse_ctx)
 
    if (jse_read_fd(STDIN_FILENO, &buffer, &size) > 0)
    {
        if (jse_run_buffer(jse_ctx, (char*)buffer, size) == DUK_EXEC_SUCCESS)
        {
            ret = 0;
        }

        free(buffer);
    }
    else
    {
        JSE_ERROR("Error: %s", strerror(errno))
        duk_push_error_object(jse_ctx->ctx, DUK_ERR_ERROR,  "%s", strerror(errno));
    }

    JSE_EXIT("run_stdin()=%d", ret)
    return ret;
}

/**
 * @brief Runs JavaScript code provided in a file.
 *
 * In case of an Error, an Error object remains on the stack when the
 * function exits.
 * 
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
static duk_int_t run_file(jse_context_t * jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;
    void * buffer = NULL;
    size_t size = 0;
    ssize_t bytes = 0;

    JSE_ENTER("run_file(%p)", jse_ctx)

    bytes = jse_read_file(jse_ctx->filename, &buffer, &size);
    if (bytes > 0)
    {
        if (jse_run_buffer(jse_ctx, (char*)buffer, size) == DUK_EXEC_SUCCESS)
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

    JSE_EXIT("run_file()=%d", ret)
    return ret;
}

/**
 * @brief Prints a string to the output.
 *
 * This function prints a string to the output. Typically this is standard
 * out. When handling HTTP requests output has to be delayed until the
 * HTTP header is output so all output is buffered internally and output
 * once the header has been generated.
 *
 * @param ctx the duktape context.
 * @return 0 or a negative error status.
 */
static duk_ret_t do_print(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    duk_int_t count = duk_get_top(ctx);

    JSE_ASSERT(ctx != NULL)
    JSE_ENTER("do_print(%p)", ctx)

    if (count > 0)
    {
        const void * dukstr = NULL;
        size_t dukstrlen = 0;

        /* Handle buffers as pure data */
        if (duk_is_buffer_data(ctx, -1))
        {
            dukstr = duk_get_buffer_data(ctx, -1, &dukstrlen);
        }
        else
        {
            dukstr = duk_safe_to_string(ctx, -1);
            dukstrlen = strlen(dukstr);
        }

        /* Need to delay output because of errors, headers, etc */
        if (is_http_request)
        {
            /* Do not use strdup because we may have null characters */
            char * string = (char *)calloc(sizeof(char), dukstrlen);
            if (string != NULL)
            {
                memcpy(string, dukstr, dukstrlen);
                print_buffer_item_t *item = (print_buffer_item_t*)calloc(sizeof(print_buffer_item_t), 1);
                if (item != NULL)
                {
                    item->string = string;
                    item->length = dukstrlen;

                    if (last_print_item != NULL)
                    {
                        last_print_item->next = item;
                    }

                    last_print_item = item;

                    if (first_print_item == NULL)
                    {
                        first_print_item = item;
                    }

                    /* Nothing returned on the value stack */
                    ret = 0;
                }
                else
                {
                    int _errno = errno;
                    free(string);
                    /* Does not return */
                    JSE_THROW_POSIX_ERROR(ctx, _errno, "calloc() failed: %s", strerror(_errno));
                }
            }
            else 
            {
                int _errno = errno;
                /* Does not return */
                JSE_THROW_POSIX_ERROR(ctx, _errno, "strdup() failed: %s", strerror(_errno));
            }
        }
        else
        {
            /* Do not use strdup because we may have null characters */
            fwrite(dukstr, dukstrlen, 1, stdout);

            /* Return undefined */
            ret = 0;
        }
    }
    else
    {
        /* Does nothing but is not an error */
        ret = 0;
    }

    JSE_EXIT("do_print()=%d", ret)
    return ret;
}

/**
 * @brief Sets the HTTP status.
 *
 * This function sets the HTTP status. It expects one integer on the duktape
 * stack that specifies the status value.
 *
 * @param ctx the duktape context.
 *
 * @return 0 or a negative error status.
 */
static duk_ret_t do_setHTTPStatus(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    JSE_ENTER("do_setHTTPStatus(%p)", ctx)

    if (duk_is_number(ctx, -1))
    {
        http_status = (int)duk_get_int_default(ctx, -1, HTTP_STATUS_OK);

        JSE_DEBUG("status = %d", http_status)

        /* Return undefined */
        ret = 0;
    }
    else
    {
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument!");
    }

    JSE_EXIT("do_setHTTPStatus()=%d", ret)
    return ret;
}

/**
 * @brief Sets the mimetype.
 *
 * This function sets the content type. It expects one string on the duktape
 * stack that specifies the content type.
 *
 * @param ctx the duktape context.
 *
 * @return 0 or a negative error status.
 */
static duk_ret_t do_setContentType(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    JSE_ENTER("do_setContentType(%p)", ctx)

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

            JSE_DEBUG("mimetype = %s", http_contenttype)

            /* Return undefined */
            ret = 0;
        }
        else 
        {
            int _errno = errno;
            /* Does not return */
            JSE_THROW_POSIX_ERROR(ctx, _errno, "strdup() failed: %s", strerror(_errno));
        }
    }
    else
    {
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument \"mimetype\"!");
    }

    JSE_EXIT("do_setContentType()=%d", ret)
    return ret;
}

/**
 * @brief Sets the cookie value.
 *
 * This function sets the cookie value. It expects five arguments, the
 * cookie name, the cookie value, an expiry time in seconds (0 expire
 * now), an optional path (may be null), an optional domain (may be
 * null), and a boolean indicating a secure cookie or not.
 *
 * @param ctx the duktape context.
 *
 * @return 0 or a negative error status.
 */
static duk_ret_t do_setCookie(duk_context * ctx)
{
    char * name = NULL;
    char * value = NULL;
    int expire_secs = 0;
    char * path = NULL;
    char * domain = NULL;
    bool secure = false;

    JSE_ENTER("do_setCookie(%p)", ctx)

    if (duk_is_string(ctx, -6))
    {
        name = strdup(duk_safe_to_string(ctx, -6));
        if (name == NULL)
        {
            int _errno = errno;
            /* Does not return */
            JSE_THROW_POSIX_ERROR(ctx, _errno, "strdup() failed: %s", strerror(_errno));
        }
    }
    else
    {
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument \"name\" (%d)", duk_get_type(ctx, -6));
    }

    if (duk_is_string(ctx, -5))
    {
        value = strdup(duk_safe_to_string(ctx, -5));
        if (value == NULL)
        {
            int _errno = errno;
            free(name);
            /* Does not return */
            JSE_THROW_POSIX_ERROR(ctx, _errno, "strdup() failed: %s", strerror(_errno));
        }
    }
    else
    {
        free(name);
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument \"value\" (%d)", duk_get_type(ctx, -5));
    }

    if (duk_is_number(ctx, -4))
    {
        expire_secs = (int)duk_get_int_default(ctx, -4, 0);
    }
    else
    {
        free(value);
        free(name);
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument \"expire_secs\" (%d)", duk_get_type(ctx, -4));
    }

    if (!duk_is_null(ctx, -3))
    {
        if (duk_is_string(ctx, -3))
        {
            path = strdup(duk_safe_to_string(ctx, -3));
            if (path == NULL)
            {
                int _errno = errno;
                free(value);
                free(name);
                /* Does not return */
                JSE_THROW_POSIX_ERROR(ctx, _errno, "strdup() failed: %s", strerror(_errno));
            }
        }
        else
        {
            free(value);
            free(name);
            /* Does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Invalid argument \"path\" (%d)", duk_get_type(ctx, -3));
        }
    }

    if (!duk_is_null(ctx, -2))
    {
        if (duk_is_string(ctx, -2))
        {
            domain = strdup(duk_safe_to_string(ctx, -2));
            if (domain == NULL)
            {
                int _errno = errno;
                free(value);
                free(name);
                free(path); /* path may be NULL but free(NULL) is a NOP per C99 */
                /* Does not return */
                JSE_THROW_POSIX_ERROR(ctx, _errno, "strdup() failed: %s", strerror(_errno));
            }
        }
        else
        {
            free(value);
            free(name);
            free(path); /* path may be NULL but free(NULL) is a NOP per C99 */
            /* Does not return */
            JSE_THROW_TYPE_ERROR(ctx, "Invalid argument \"domain\" (%d)", duk_get_type(ctx, -2));
        }
    }

    if (duk_is_boolean(ctx, -1))
    {
        /* Prevent error with -Wbad-function-cast */
        duk_bool_t duksec = duk_get_boolean_default(ctx, -1, false);
        secure = (bool)duksec;
    }
    else
    {
        free(value);
        free(name);
        free(path);   /* path may be NULL but free(NULL) is a NOP per C99 */
        free(domain); /* domain may be NULL but free(NULL) is a NOP per C99 */
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument \"secure\" (%d)", duk_get_type(ctx, -1));
    }

    cookie_destroy(&http_cookie);
    cookie_set(&http_cookie, name, value, expire_secs, path, domain, secure);

    JSE_DEBUG("cookie=\"%s\"=\"%s\",expires=%d,path=\"%s\",domain=\"%s\",secure=%s",
        http_cookie.name, http_cookie.value, http_cookie.expire_secs,
        http_cookie.path != NULL ? http_cookie.path : "(null)",
        http_cookie.domain !=NULL ? http_cookie.domain : "(null)",
        http_cookie.secure ? "true" : "false")

    JSE_EXIT("do_setCookie()=0")
    return 0;
}

/**
 * @brief Sets or adds an HTTP header
 * 
 * This function sets or adds a header. The function expects two strings
 * the first being the header name. The second being the value. If the
 * name already exists it is replaced by the new value. The test for the
 * name is case insensitive.
 *
 * @param ctx the duktape context.
 *
 * @return 0 or a negative error status.
 */
static duk_ret_t do_setHeader(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;
    char * name = NULL;
    char * value = NULL;

    JSE_ENTER("do_setHeader(%p)", ctx)

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
                            int _errno = errno;
                            free(name);
                            /* Does not return */
                            JSE_THROW_POSIX_ERROR(ctx, _errno, "strdup() failed: %s", strerror(_errno));
                        }
                    }
                    else
                    {
                        free(name);
                        /* Does not return */
                        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument: value (%d)", duk_get_type(ctx, -1));
                    }
                }
            }
            else
            {
                free(name);
                /* Does not return */
                JSE_THROW_TYPE_ERROR(ctx, "Illegal header name: \"%s\"", name);
            }
        }
        else
        {
            int _errno = errno;
            /* Does not return */
            JSE_THROW_POSIX_ERROR(ctx, _errno, "strdup() failed: %s", strerror(_errno));
        }
    }
    else
    {
        /* Does not return */
        JSE_THROW_TYPE_ERROR(ctx, "Invalid argument \"name\" (%d)", duk_get_type(ctx, -2));
    }

    JSE_EXIT("do_setHeader()=%d", ret)
    return ret;
}

/**
 * @brief Binds external functions to the JavaScript engine.
 *
 * @param jse_ctx the jse context.
 * @return an error code or 0.
 */
static duk_int_t bind_functions(jse_context_t *jse_ctx)
{
    duk_int_t ret = 0;

    JSE_ASSERT(jse_ctx != NULL)
    JSE_ENTER("bind_functions(%p)", jse_ctx)

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
        JSE_ERROR("Failed to bind jscommon functions!")
    }
    else if ((ret = jse_bind_jsprocess(jse_ctx)) != 0)
    {
        JSE_ERROR("Failed to bind jsprocess functions!")
    }
#ifdef ENABLE_LIBXML2
    else if ((ret = jse_bind_xml(jse_ctx)) != 0)
    {
        JSE_ERROR("Failed to bind xml functions!")
    }
#endif
#ifdef BUILD_RDK
    else if ((ret = jse_bind_cosa(jse_ctx)) != 0)
    {
        JSE_ERROR("Failed to bind cosa functions!")
    }
#endif
#ifdef ENABLE_LIBCRYPTO
    else if ((ret = jse_bind_crypt(jse_ctx)) != 0)
    {
        JSE_ERROR("Failed to bind xml functions!")
    }
#endif

    JSE_EXIT("bind_functions()=%d", ret)
    return ret;
}

/**
 * @brief Unbinds functions from the javascript engine.
 *
 * @param jse_ctx the jse context.
 */
static void unbind_functions(jse_context_t *jse_ctx)
{
#ifdef ENABLE_LIBCRYPTO
    jse_unbind_crypt(jse_ctx);
#endif
#ifdef BUILD_RDK
    jse_unbind_cosa(jse_ctx);
#endif
#ifdef ENABLE_LIBXML2
    jse_unbind_xml(jse_ctx);
#endif
    jse_unbind_jsprocess(jse_ctx);
    jse_unbind_jscommon(jse_ctx);
}

/**
 * @brief Utility method that adds an environment variable as an object property
 *
 * Given the object at idx, this function adds the environment variable
 * env, given it is defined, as a property called env with the value of
 * the environment variable env.
 *
 * @param ctx the duktape context.
 * @param idx the request object index.
 * @param env the environment variable name.
 *
 * @return 0.
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
 * @brief Utility method that adds the query parameters to the request object
 *
 * Given the object at idex, this function adds a sub-object "QueryParameters"
 * and adds properties to that object with the name and value of each of the
 * query parameters parsed by QDecoder.
 *
 * @param ctx the duktape context.
 * @param req the QDecoder request list.
 * @param idx the request object index.
 */
static void create_request_object_params(duk_context *ctx, qentry_t *req, duk_int_t idx)
{
    struct file_object_s *file_objs = NULL;
    struct file_object_s *file_obj =  NULL;
    qentobj_t obj;
    duk_idx_t idx2;
    char *propName;

    JSE_ENTER("create_request_object_params(%p, %p, %d)", ctx, req, idx)

    memset(&obj, 0, sizeof(obj));

    idx2 = duk_push_object(ctx);

    JSE_VERBOSE("idx2=%d", idx2);

    while (req->getnext(req, &obj, NULL, false))
    {
        JSE_DEBUG("Parameter: name=\"%s\", value=\"%s\"", obj.name, (char*)obj.data)

        // File uploads result in several parameters with the upload name
        // followed by . followed by property, e.g. filename, mimetype etc.
        propName = strchr(obj.name, '.');
        if (propName != NULL)
        {
            file_obj = file_objs;

            *propName++ = '\0';
            while (file_obj)
            {
                if (!strcmp(obj.name, file_obj->name))
                {
                    break;
                }

                file_obj = file_obj->next;
            }

            JSE_VERBOSE("file_obj=%p", file_obj)

            if (file_obj == NULL)
            {
                // Create a new file object
                file_obj = (struct file_object_s*)calloc(sizeof(struct file_object_s), 1);
                if (file_obj != NULL)
                {
                    JSE_DEBUG("Creating object: %s", obj.name)

                    file_obj->name = strdup(obj.name);
                    if (file_obj->name != NULL)
                    {
                        // Create the object and add it
                        file_obj->idx = duk_push_object(ctx);

                        // You can't put an object as a property of a parent
                        // object until all its own properties are set. So no
                        // duk_put_prop_string(ctx, idx, "QueryParameters");
                        // here.

                        // Link in the new object
                        file_obj->next = file_objs;
                        file_objs = file_obj;
                    }
                    else
                    {
                        JSE_ERROR("strdup() failed: %s", strerror(errno))
                        free(file_obj);

                        // Skip on to next param
                        continue;
                    }
                }
                else
                {
                    JSE_ERROR("calloc() failed: %s", strerror(errno))

                    // Skip on to next param
                    continue;
                }
            }

            JSE_VERBOSE("file_obj=%p", file_obj)

            if (file_obj != NULL)
            {
                JSE_VERBOSE("Adding file property: %s (idx=%d)", propName, file_obj->idx)

                // Add the property to the file object
                duk_push_string(ctx, (char*)obj.data);
                duk_put_prop_string(ctx, file_obj->idx, propName);
            }
        }
        else
        {
            // Simple case for a string
            duk_push_string(ctx, (char*)obj.data);
            duk_put_prop_string(ctx, idx2, obj.name);
        }
    }

    // Clean up temp data and set properties. This should be in reverse
    // order to them being added. I.e. the first file object pushed on to
    // the stack will be the last one read.
    while (file_objs)
    {
        file_obj  = file_objs;
        file_objs = file_objs->next;

        JSE_VERBOSE("Adding file as property of QueryParameters: %s (idx=%d)", file_obj->name, idx2)

        duk_put_prop_string(ctx, idx2, file_obj->name);
        free(file_obj->name);
        free(file_obj);
    }

    duk_put_prop_string(ctx, idx, "QueryParameters");

    JSE_EXIT("create_request_object_params()")
}

/**
 * @brief Adds the CGI variables as properties to the request object.
 *
 * This function adds properties to the "Request" object with the name
 * and value of each of the CGI environment variables.
 *
 * @param ctx the duktape context.
 * @param idx the request object index.
 */
static void create_request_object_envs(duk_context *ctx, duk_int_t idx)
{
    int i;

    add_env_as_object_property(ctx, idx, "CONTENT_LENGTH");
    add_env_as_object_property(ctx, idx, "CONTENT_TYPE");
    add_env_as_object_property(ctx, idx, "DOCUMENT_ROOT");
    add_env_as_object_property(ctx, idx, "HTTPS");
    add_env_as_object_property(ctx, idx, "PATH");
    add_env_as_object_property(ctx, idx, "QUERY_STRING");
    add_env_as_object_property(ctx, idx, "REMOTE_ADDR");
    add_env_as_object_property(ctx, idx, "REMOTE_HOST");
    add_env_as_object_property(ctx, idx, "REMOTE_PORT");
    add_env_as_object_property(ctx, idx, "REMOTE_USER");
    add_env_as_object_property(ctx, idx, "REQUEST_METHOD");
    add_env_as_object_property(ctx, idx, "REQUEST_URI");
    add_env_as_object_property(ctx, idx, "SCRIPT_FILENAME");
    add_env_as_object_property(ctx, idx, "SCRIPT_NAME");
    add_env_as_object_property(ctx, idx, "SERVER_NAME");
    add_env_as_object_property(ctx, idx, "SERVER_PORT");

    /* environ is a NULL terminated list of pointers */

    /* Now add all the HTTP_ environment variables. All request
       headers are converted in to HTTP_ environment variables
       by the server by capitalisation of the header name and
       prepending HTTP_ to it. */
    for (i = 0; environ[i] != NULL; i++)
    {
        /* If the variable starts with HTTP_ process it */
        if (0 == strncmp(environ[i], "HTTP_", CONST_STRLEN("HTTP_")))
        {
            /* Get a modifiable version of the name=value pair. */
            char * var = strdup(environ[i]);
            if (var == NULL)
            {
                JSE_ERROR("strdup() failed: %s", strerror(errno))
            }
            else
            {
                char * pequals = strchr(var, '=');
                if (pequals != NULL)
                {
                    *pequals = 0;
                    add_env_as_object_property(ctx, idx, var);
                }

                free(var);
            }
        }
    }
}

/**
 * @brief Indicates if the request has a body to parse
 *
 * This function returns true if the request has a body to parse. I.e. the
 * request is either a PUT, POST or PATCH.
 *
 * @return true if there is a request body to parse
 */
static bool request_has_body(void)
{
    bool hasBody = false;

    JSE_ENTER("request_has_body()")

    char * requestMethod = getenv("REQUEST_METHOD");
    if (requestMethod != NULL)
    {
        if (0 == strcmp(requestMethod, "PUT")  ||
            0 == strcmp(requestMethod, "POST") ||
            0 == strcmp(requestMethod, "PATCH"))
        {
            hasBody = true;
        }
    }

    JSE_EXIT("request_has_body()=%s", hasBody ? "true" : "false");
    return hasBody;
}

/**
 * @brief Indicates if the content is parsable by qdecoder
 *
 * QDecoder can parse the following mime types:
 *
 *  - "application/x-www-form-urlencoded"
 *  - "multipart/form-data"
 *
 * @return true if parsable by qdecoder.
 */
static bool request_parsable_by_qdecoder(void)
{
    bool parsable = false;

    JSE_ENTER("request_parsable_by_qdecoder()")

    /* If qdecoder can parse the content let it. */
    const char * contentType = getenv("CONTENT_TYPE");
    if (contentType != NULL)
    {
        if (0 == strncmp(contentType,
                "application/x-www-form-urlencoded",
                CONST_STRLEN("application/x-www-form-urlencoded")) ||
            0 == strncmp(contentType,
                "multipart/form-data",
                CONST_STRLEN("multipart/form-data")))
        {
            parsable = true;
        }
    }

    JSE_EXIT("request_parsable_by_qdecoder()=%s", parsable ? "true" : "false");
    return parsable;
}

/**
 * @brief Returns the request body content length
 *
 * This function returns the request body content length or -1 if it is not
 * defined or is defined but not set in the header.
 *
 * @return the request body content length or -1 if not set
 */
static int get_request_content_length(void)
{
    const char * contentLength = getenv("CONTENT_LENGTH");
    int length = -1;

    JSE_ENTER("get_request_content_length()")

    if (contentLength != NULL)
    {
        length = atoi(contentLength);
    }

    JSE_EXIT("get_request_content_length()=%d", length);
    return length;
}

/**
 * @brief Creates a property comprising the HTTP request body.
 *
 * This function creates a property that comprises the body of the request.
 * The property is only created if qdecoder does not handle it. QDecoder can
 * parse the following mime types:
 *
 *  - "application/x-www-form-urlencoded"
 *  - "multipart/form-data"
 *
 * If qdecoder cannot handle the mimetype the request body is read in to a
 * property called "body" so that it can be processed by a script.
 *
 * @param ctx the duktape context.
 * @param idx the index of the Request object.
 */
static void create_request_object_body(duk_context *ctx, duk_int_t idx)
{
    JSE_ENTER("create_request_object_body()")

    /* Is it a POST, PUT or PATCH? */
    if (request_has_body())
    {
        /* If qdecoder doesn't handle it, read the body and return it. */
        if (!request_parsable_by_qdecoder())
        {
            /* Negative content length means not set */
            int length = get_request_content_length();
            if (length > 0)
            {
                /* Easy case. We know the size. */
                size_t size = (size_t)length;

                char * buffer = (char*)calloc(size, sizeof(char));
                if (buffer != NULL)
                {
                    /* Use buffered streams because FCGI redirects these and
                       we want to read from FCGI's stdin when enabled. */
                    size_t bytes = fread(buffer, 1, size, stdin);

                    /* bytes short when error or EOF */
                    if (bytes < size && ferror(stdin))
                    {
                        JSE_ERROR("read() failed: %s", strerror(errno))
                    }
                    else
                    {
                        /* EOF is fine */
                        duk_push_lstring(ctx, buffer, bytes);
                        duk_put_prop_string(ctx, idx, "Body");
                    }

                    free(buffer);
                }
            }
            /* else no body to return */
        }
    }

    JSE_EXIT("create_request_object_body()")
}

/**
 * @brief Creates a request object.
 *
 * Creates a global JavaScipt object called "Request" which comprises the
 * HTTP request data including the environment variables, query parameters
 * etc.
 *
 * @param jse_ctx the jse context.
 *
 * @return 0.
 */
static duk_int_t create_request_object(jse_context_t *jse_ctx)
{
    duk_context *ctx = jse_ctx->ctx;
    qentry_t *req = jse_ctx->req;
    duk_int_t idx = duk_push_object(ctx);

    create_request_object_params(ctx, req, idx);
    create_request_object_envs(ctx, idx);
    create_request_object_body(ctx, idx);

    duk_put_global_string(ctx, JSE_REQUEST_OBJECT_NAME);

    return 0;
}

 /**
 * @brief Returns a simple HTTP server error response.
 *
 * @param status the HTTP status.
 * @param msg the message.
 */
static void basic_return_error(int status)
{
    const char * msg = msg_for_http_status(status);

    printf("Status: %d %s\r\nContent-Type: text/plain\r\n\r\n%s\r\n", status, msg, msg);
}

/**
 * @brief Handle an HTTP request
 *
 * A note on error return codes.
 *
 * Returning an error from this function means the an error occurred and it
 * was not reported to the caller. This means that if in CGI mode and
 * qdecoder was initialised correctly an error will be reported as an HTTP
 * error status and the function will return success. Similarly if in
 * command line mode and the error is a script error and reported on stderr,
 * the function will return success.
 *
 * So if this function returns an error status it should be handled.
 *
 * @param jse_ctx the jse context.
 * @return an error code or 0.
 */
static duk_int_t handle_request(jse_context_t *jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_ENTER("handle_request(%p)", jse_ctx)

    ret = bind_functions(jse_ctx); 
    if (ret == 0)
    {
        /* Set the file upload options (POST only) */
        if (process_post)
        {
            jse_ctx->req = qcgireq_setoption(jse_ctx->req, true, upload_dir, JSE_UPLOAD_EXPIRY_SECS);
        }

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

            ret = 0;
        }
        /* An HTTP method was set but we failed to parse. */
        else if (process_post || process_get || process_cookie)
        {
            jse_ctx->req = qcgireq_parse(jse_ctx->req, Q_CGI_GET);
            if (jse_ctx->req != NULL)
            {
                return_error(jse_ctx, HTTP_STATUS_METHOD_NOT_ALLOWED, "text/html", 
                    "<html><head><title>Method not allowed</title></head><body>Method not allowed</body></html>");
            }
            else
            {
                JSE_ERROR("qcgireq_parse() failed!")
                basic_return_error(HTTP_STATUS_METHOD_NOT_ALLOWED);
            }

            ret = 0;
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

                /* Exit the process */
                exit(EXIT_FATAL);
            }
        }

        /* clean up qdecoder */
        if (jse_ctx->req != NULL)
        {
            jse_ctx->req->free(jse_ctx->req);
            jse_ctx->req = NULL;
        }

        unbind_functions(jse_ctx);
    }
    else
    {
        JSE_ERROR("bind_functions() failed!")
    }

    JSE_EXIT("handle_request()=%d", ret)
    return ret;
}

/**
 * Displays help on stderr.
 */
static void help(const char* name)
{
    fprintf(stderr,
"Usage: %s [OPTION]... [FILENAME]\n"
"Run a JavaScript script as a CGI request.\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -c, --cookies            Handle cookie requests.\n"
"  -e, --enter-exit         Enable enter-exit debug logging.\n"
"  -g, --get                Handle GET requests.\n"
"  -h, --help               Display this help.\n"
"  -p, --post               Handle POST requests.\n"
"  -u, --upload-dir         Override the default file upload directory.\n"
"  -v, --verbose            Verbosity. Multiple uses increases vebosity.\n"
#ifdef BUILD_RDK
"  -n, --no-ccsp            Do not initialise CCSP.\n"
#endif
"\n"
"Exit status:\n"
" 0  if OK,\n"
" 1  if an ERROR,\n"
" 2  if a FATAL error.\n", name);
}

/**
 * main!
 * 
 * @param argc the argument count
 * @param argv the array of argument strings
 *
 * @return 0 or an error code.
 */
int main(int argc, char **argv)
{
    char * jseargs = NULL;
    char * filename = NULL;
    char * arg = NULL;
    duk_int_t ret = DUK_ERR_ERROR;
    
#ifdef BUILD_RDK
    /* By default we initialise CCSP unless turned off on the command line */
    bool init_ccsp = true;
#endif
    JSE_DEBUG_INIT()
    JSE_VERBOSE("main()")
    
    while (1)
    {
        int c, option_index = 0;
        static struct option long_options[] =
        {
            {"cookies",     no_argument,       0, 'c' },
            {"enter-exit",  no_argument,       0, 'e' },
            {"get",         no_argument,       0, 'g' },
            {"help",        no_argument,       0, 'h' },
#ifdef BUILD_RDK
            {"no-ccsp",     no_argument,       0, 'n' },
#endif
            {"post",        no_argument,       0, 'p' },
            {"upload-dir",  required_argument, 0, 'u' },
            {"verbose",     no_argument,       0, 'v' },
            {0,             0,                 0,  0  }
        };

#ifdef JSE_DEBUG_ENABLED
        c = getopt_long(argc, argv, "ceghnpu:v", long_options, &option_index);
#else
        c = getopt_long(argc, argv, "cghnpu:", long_options, &option_index);
#endif
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

#ifdef JSE_DEBUG_ENABLED
            case 'e':
                JSE_DEBUG("Enter/Exit debug enabled!")
                jse_enter_exit = true;
                break;
#endif

            case 'g':
                JSE_DEBUG("GET processing enabled!")
                process_get = true;
                break;

            case 'h':
                help(argv[0]);
                exit(EXIT_SUCCESS);

#ifdef BUILD_RDK
            case 'n':
                JSE_DEBUG("CCSP init disabled")
                init_ccsp = false;
                break;
#endif

            case 'p':
                JSE_DEBUG("POST processing enabled!")
                process_post = true;
                break;

            case 'u':
                JSE_DEBUG("Upload directory: %s", optarg)
                upload_dir = strdup(optarg);
                break;

#ifdef JSE_DEBUG_ENABLED
            case 'v':
                jse_verbosity ++;
                break;
#endif

            default:
                JSE_ERROR("Invalid option")
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        JSE_DEBUG("Filename: \"%s\"", argv[optind])
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
#ifdef JSE_DEBUG_ENABLED
            else
            if (!strcmp("-e", arg) || !strcmp("--enter-exit", arg))
            {
                jse_enter_exit = true;
            }
#endif
            else
            if (!strcmp("-g", arg) || !strcmp("--get", arg))
            {
                process_get = true;
            }
#ifdef BUILD_RDK
            else
            if (!strcmp("-n", arg) || !strcmp("--no-ccsp", arg))
            {
                init_ccsp = false;
            }
#endif
            else
            if (!strncmp("-u", arg, 2))
            {
                if (strlen(arg) > 2)
                {
                    /* path immediately follows option */
                    arg += 2;
                }
                else
                {
                    arg = strtok(NULL, " ");
                }
                
                if (arg == NULL) {
                    JSE_WARNING("Missing parameter for -u")
                    break;
                }

                upload_dir = strdup(arg);
            }
            else 
            if (!strcmp("--upload-dir", arg))
            {
                arg = strtok(NULL, " ");
                if (arg == NULL) {
                    JSE_WARNING("Missing parameter for --upload-dir")
                    break;
                }

                upload_dir = strdup(arg);
            }
            else
            if (!strcmp("-p", arg) || !strcmp("--post", arg))
            {
                process_post = true;
            }
#ifdef JSE_DEBUG_ENABLED
            else
            if (!strcmp("-v", arg) || !strcmp("--verbose", arg))
            {
                jse_verbosity ++;
            }
#endif

            arg = strtok(NULL, " ");
        }
    }

    // Post only
    if (process_post)
    {
        if (upload_dir == NULL) {
            upload_dir = JSE_UPLOAD_DIR;
        }

        if (jse_mkdir(upload_dir))
        {
            exit(EXIT_FATAL);
        }
    }

/* Disable initialisation on start up for FCGI for now. */
#if defined(BUILD_RDK) && !defined(ENABLE_FASTCGI)
    /* Try and initialise cosa before the main processing */
    if (init_ccsp)
    {
        JSE_INFO("Initialising COSA!")

        ret = jse_cosa_init();
        if (ret != 0)
        {
            /* Failure isn't terminal. */
            JSE_WARNING("jse_cosa_init() failed. Will try again later!");
        }
        else
        {
            cosa_initialised = true;
        }
    }
#endif

#ifdef ENABLE_FASTCGI
    while (FCGI_Accept() >= 0)
    {
        ret = DUK_ERR_ERROR;

        JSE_INFO("FCGI loop start")

        /* For Fast CGI get the script file name from the environment */
        char *filenameenv = getenv("SCRIPT_FILENAME");
        if (filenameenv == NULL)
        {
            JSE_ERROR("SCRIPT_FILENAME is NULL!")

            basic_return_error(HTTP_STATUS_INTERNAL_SERVER_ERROR);
            continue;
        }

        /* Need a copy of the string - will be freed by jse_context_destroy */
        filename = strdup(filenameenv);
        if (filename == NULL)
        {
            JSE_ERROR("strdup() failed: %s", strerror(errno))

            basic_return_error(HTTP_STATUS_INTERNAL_SERVER_ERROR);
            continue;
        }

        JSE_INFO("Script filename: %s", filename)

#ifdef BUILD_RDK
        /* If we didn't successfully initialise CCSP Cosa but we wanted to
           lets try again! */
        if (init_ccsp && !cosa_initialised)
        {
            JSE_INFO("Initialising COSA!")

            ret = jse_cosa_init();
            if (ret != 0)
            {
                JSE_WARNING("jse_cosa_init() failed. Will try again later!");

                basic_return_error(HTTP_STATUS_INTERNAL_SERVER_ERROR);
                continue;
            }
            else
            {
                cosa_initialised = true;
            }
        }
#endif
#endif

        /* Get the script and process it */
        jse_context_t *jse_ctx = jse_context_create(filename);
        if (jse_ctx != NULL)
        {
            if (init_duktape(jse_ctx) != NULL)
            {
                if (handle_request(jse_ctx) == 0)
                {
                    ret = 0;
                }

                cleanup_duktape(jse_ctx);
            }

            jse_context_destroy(jse_ctx);
        }
        else
        {
            ret = DUK_ERR_ERROR;
        }

        if ((ret != 0) && (process_get || process_post || process_cookie))
        {
            basic_return_error(HTTP_STATUS_INTERNAL_SERVER_ERROR);
            ret = 0;
        }

#ifdef ENABLE_FASTCGI
        JSE_INFO("FCGI loop end")
    }
#endif

    /* An error is returned only when it hasn't already been handled. */
    if (ret != 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
