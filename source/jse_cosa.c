/*
 If not stated otherwise in this file or this component's Licenses.txt file the
 following copyright and licenses apply:

 Copyright 2018 RDK Management

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <ccsp_message_bus.h>
#include <ccsp_base_api.h>
#include <ccsp_memory.h>
#include <ccsp_custom.h>
#include <duktape.h>

#include "jse_debug.h"
#include "jse_jserror.h"
#include "jse_cosa_error.h"
#include "jse_cosa.h"

/** Reference count for binding. */
static int ref_count = 0;

#define COMPONENT_NAME "ccsp.phpextension"
#define CONF_FILENAME "/tmp/ccsp_msg.cfg"
#define CCSP_COMPONENT_ID_WebUI 0x00000001
#define COSA_PHP_EXT_PCSIM "/tmp/cosa_php_pcsim"

void *bus_handle = NULL;
char *dst_componentid = NULL;
char *dst_pathname = NULL;
char dst_pathname_cr[64] = {0};
static int gPcSim = 0;

#ifndef BUILD_RBUS /*FIXME: mrollins: completely removed the functionality when rbus enabled -- do we need to add it back with rbus ? */
static const char *msg_path = "/com/cisco/spvtg/ccsp/phpext";
static const char *msg_interface = "com.cisco.spvtg.ccsp.phpext";
static const char *msg_method = "__send";
static const char *Introspect_msg = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
                                    "<node name=\"/com/cisco/ccsp/dbus\">\n"
                                    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
                                    "    <method name=\"Introspect\">\n"
                                    "      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
                                    "    </method>\n"
                                    "  </interface>\n"
                                    "  <interface name=\"ccsp.msg\">\n"
                                    "    <method name=\"__send\">\n"
                                    "      <arg type=\"s\" name=\"from\" direction=\"in\" />\n"
                                    "      <arg type=\"s\" name=\"request\" direction=\"in\" />\n"
                                    "      <arg type=\"s\" name=\"response\" direction=\"out\" />\n"
                                    "    </method>\n"
                                    "    <method name=\"__app_request\">\n"
                                    "      <arg type=\"s\" name=\"from\" direction=\"in\" />\n"
                                    "      <arg type=\"s\" name=\"request\" direction=\"in\" />\n"
                                    "      <arg type=\"s\" name=\"argu\" direction=\"in\" />\n"
                                    "      <arg type=\"s\" name=\"response\" direction=\"out\" />\n"
                                    "    </method>\n"
                                    "  </interface>\n"
                                    "</node>\n";

/**
 * @brief Dbus message handler
 *
 * @param conn connection object
 * @param message dbus message
 * @param user_data
 * @return an error status or DBUS_HANDLER_RESULT_HANDLED.
 */
static DBusHandlerResult path_message_func(DBusConnection *conn, DBusMessage *message, void *user_data)
{
    const char *interface = dbus_message_get_interface(message);
    const char *method = dbus_message_get_member(message);
    DBusMessage *reply;
    char *resp = "888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888";
    char *from = 0;
    char *req = 0;
    char *err_msg = DBUS_ERROR_NOT_SUPPORTED;

    /* To keep compiler happy */
    user_data = user_data;

    reply = dbus_message_new_method_return(message);
    if (reply == NULL)
    {
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (!strcmp("org.freedesktop.DBus.Introspectable", interface) && !strcmp(method, "Introspect"))
    {
        if (!dbus_message_append_args(reply, DBUS_TYPE_STRING, &Introspect_msg, DBUS_TYPE_INVALID))
        {
            if (!dbus_connection_send(conn, reply, NULL))
            {
                dbus_message_unref(reply);
            }
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (!strcmp(msg_interface, interface) && !strcmp(method, msg_method))
    {
        if (dbus_message_get_args(message,
                                  NULL,
                                  DBUS_TYPE_STRING, &from,
                                  DBUS_TYPE_STRING, &req,
                                  DBUS_TYPE_INVALID))
        {
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &resp, DBUS_TYPE_INVALID);
            if (!dbus_connection_send(conn, reply, NULL))
            {
                dbus_message_unref(reply);
            }
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    dbus_message_set_error_name(reply, err_msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}
#endif /* BUILD_RBUS */

/**
 * @brief Locate component for DM key/parameter
 *
 * CCSP API call: CcspBaseIf_discComponentSupportingNamespace()
 *
 * @param pObjName object name
 * @param ppDestComponentName pointer to dest component name
 * @param ppDestPath pointer to component dbus path
 * @param pSystemPrefix subsystem prefix
 * @return an error status or 0.
 */
static int UiDbusClientGetDestComponent(char *pObjName, char **ppDestComponentName, char **ppDestPath, char *pSystemPrefix)
{
    int ret;
    int size = 0;
    componentStruct_t **ppComponents = NULL;

    ret =
        CcspBaseIf_discComponentSupportingNamespace(
            bus_handle,
            dst_pathname_cr,
            pObjName,
            pSystemPrefix,
            &ppComponents,
            &size);

    if (ret == CCSP_SUCCESS)
    {
        *ppDestComponentName = ppComponents[0]->componentName;
        ppComponents[0]->componentName = NULL;
        *ppDestPath = ppComponents[0]->dbusPath;
        ppComponents[0]->dbusPath = NULL;

        while (size)
        {
            if (ppComponents[size - 1]->remoteCR_dbus_path)
            {
                free(ppComponents[size - 1]->remoteCR_dbus_path);
            }
            if (ppComponents[size - 1]->remoteCR_name)
            {
                free(ppComponents[size - 1]->remoteCR_name);
            }
            free(ppComponents[size - 1]);
            size--;
        }
        return 0;
    }
    else
    {
        JSE_ERROR(
            "Failed to locate the component for %s%s%s, error code = %d!",
            pSystemPrefix,
            strlen(pSystemPrefix) ? "." : "",
            pObjName,
            ret)
        return ret;
    }
}

/**
 * @brief Parse and set DM subsystem prefix.
 *
 * @param ppDotStr pointer to COSA dotstr
 * @param pSubSystemPrefix subsystem string, e.g. eRT.
 * @return void.
 */
static void CheckAndSetSubsystemPrefix(char **ppDotStr, char *pSubSystemPrefix)
{
    if (!strncmp(*ppDotStr, "eRT", 3)) /* check whether str has prex of eRT */
    {
        /* Replace strncpy() to prevent compiler warnings */
        pSubSystemPrefix[0] = 'e';
        pSubSystemPrefix[1] = 'R';
        pSubSystemPrefix[2] = 'T';
        pSubSystemPrefix[3] = '.';
        *ppDotStr += 4; /* shift four bytes to get rid of eRT: */
    }
    else if (!strncmp(*ppDotStr, "eMG", 3)) /* check wither str has prex of eMG */
    {
        /* Replace strncpy() to prevent compiler warnings */
        pSubSystemPrefix[0] = 'e';
        pSubSystemPrefix[1] = 'M';
        pSubSystemPrefix[2] = 'G';
        pSubSystemPrefix[3] = '.';
        *ppDotStr += 4; /* shift four bytes to get rid of eMG; */
    }
}

/**
 * @brief Initialise CCSP message bus
 *
 * @return an error status or 0.
 */
int jse_cosa_init()
{
    FILE *fp = NULL;
    int returnStatus = 0;
    int ret = -1;

    JSE_ENTER("jse_cosa_init()")

    /* Check if this is a PC simulation */
    fp = fopen(COSA_PHP_EXT_PCSIM, "r");
    if (fp)
    {
        gPcSim = 1;
        fclose(fp);
    }

    bus_handle = NULL;

    /*
     *  Hardcoding "eRT." is just a workaround. We need to feed the subsystem
     *  info into this initialization routine.
     */
    if (gPcSim)
    {
        JSE_VERBOSE("COSA using PC simulator!")
        strncat(dst_pathname_cr, CCSP_DBUS_INTERFACE_CR, sizeof(dst_pathname_cr));
    }
    else
    {
        strncat(dst_pathname_cr, "eRT." CCSP_DBUS_INTERFACE_CR, sizeof(dst_pathname_cr));
    }

    returnStatus = CCSP_Message_Bus_Init(COMPONENT_NAME, CONF_FILENAME, &bus_handle, 0, 0);
    if (returnStatus != 0)
    {
        JSE_ERROR("Message bus init failed, error code = %d!", returnStatus)
    }
    else
    {
#ifdef BUILD_RBUS
        ret = 0;
#else
        (void) CCSP_Msg_SleepInMilliSeconds(1000);
        returnStatus = CCSP_Message_Bus_Register_Path(bus_handle, msg_path, path_message_func, 0);
        if (returnStatus != CCSP_Message_Bus_OK)
        {
            JSE_ERROR("Message bus register failed, error code = %d!", returnStatus)
            CCSP_Message_Bus_Exit(bus_handle);
            bus_handle = NULL;
        }
        else
        {
            JSE_INFO("COSA initialised!")
            ret = 0;
        }
#endif
    }

    JSE_EXIT("jse_cosa_init()=%d", ret)
    return ret;
}

/**
 * Shutdown the CCSP message bus
 */
void jse_cosa_shutdown()
{
    JSE_ENTER("jse_cosa_shutdown()")

/* TODO fix ccsp_message_bus.h: it doesn't define CCSP_Message_Bus_Exit if BUILD_RBUS enabled */
#ifndef BUILD_RBUS
    if (bus_handle)
    {
        CCSP_Message_Bus_Exit(bus_handle);
        bus_handle = NULL;
    }
#endif

    JSE_EXIT("jse_cosa_shutdown()");
}

/**
 * @brief Helper function to count JS array size
 *
 * @param ctx the duktape context.
 * @param i duktape array index.
 * @return an error status or DUK_EXEC_SUCCESS.
 */
static int get_array_length(duk_context *ctx, duk_idx_t i)
{
    int len = 0;
    duk_enum(ctx, i, 0);
    while (duk_next(ctx, -1, 0))
    {
        len++;
        duk_pop(ctx);
    }
    duk_pop(ctx);
    return len;
}

/**
 * @brief Utility function for parsing JS calling args.
 *
 * @param func name of bind function.
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t parse_parameter(const char *func, duk_context *ctx, const char *types, ...)
{
    static const int overrun_guard = 10;
    duk_ret_t ret = 0;
    va_list vl;
    int i;

    JSE_ASSERT(func != NULL)
    JSE_ASSERT(ctx != NULL)
    JSE_ASSERT(types != NULL)

    va_start(vl, types);

    for (i = 0; types[i] && i < overrun_guard && (ret == 0); ++i)
    {
        switch (types[i])
        {
            /* string */
            case 's':
                if (!duk_is_string(ctx, i))
                {
                    /* Does not return */
                    JSE_THROW_TYPE_ERROR(ctx, "Type 's' parameter specified is not a string!");
                }
                else
                {
                    const char **pstr = va_arg(vl, const char **);
                    if (pstr == NULL)
                    {
                        JSE_ERROR("pstr is NULL")
                        ret = DUK_RET_ERROR;
                        break;
                    }
                    else
                    {
                        *pstr = duk_get_string(ctx, i);
                        if (*pstr)
                        {
                            if (!strlen(*pstr))
                            {
                                /* Does not return */
                                JSE_THROW_TYPE_ERROR(ctx, "%s - parameter %d (string) empty", func, i);
                            }
                        }
                        else
                        {
                            /* Does not return */
                            JSE_THROW_TYPE_ERROR(ctx, "%s - parameter %d (string) missing", func, i);
                        }
                    }
                }
                break;

            /* boolean */
            case 'b':
                if (!duk_is_boolean(ctx, i))
                {
                    /* Does not return */
                    JSE_THROW_TYPE_ERROR(ctx, "Type 'b' parameter specified is not a boolean!");
                }
                else
                {
                    duk_bool_t *pbool = (duk_bool_t*)va_arg(vl, bool *);
                    if (pbool == NULL)
                    {
                        JSE_ERROR("pbool is NULL")
                        ret = DUK_RET_ERROR;
                        break;
                    }
                    else
                    {
                        *pbool = duk_get_boolean(ctx, i);
                    }
                }
                break;

            /* object, e.g. JS object/array/function or duktape thread/internal object */
            case 'o':
                if (!duk_is_object(ctx, i))
                {
                    /* Does not return */
                    JSE_THROW_TYPE_ERROR(ctx, "Type 'o' parameter specified is not an object!");
                }
                else
                {
                    duk_idx_t *pidx = va_arg(vl, int *);
                    if (pidx == NULL)
                    {
                        JSE_ERROR("pidx is NULL")
                        ret = DUK_RET_ERROR;
                        break;
                    }
                    else
                    {
                        *pidx = i;
                    }
                }
                break;

            /* number */
            case 'n':
                if (!duk_is_number(ctx, i))
                {
                    /* Does not return */
                    JSE_THROW_TYPE_ERROR(ctx, "Type 'n' parameter specified is not a number!");
                }
                else
                {
                    duk_double_t *pdouble = va_arg(vl, double *);
                    if (pdouble == NULL)
                    {
                        JSE_ERROR("pdouble is NULL")
                        ret = DUK_RET_ERROR;
                        break;
                    }
                    else
                    {
                        *pdouble = duk_get_number(ctx, i);
                    }
                }
                break;

            /* any (except null or undefined) - coerces value to a string */
            case 'a':
                if (duk_is_null_or_undefined(ctx, i))
                {
                    /* Does not return */
                    JSE_THROW_TYPE_ERROR(ctx, "Type 'a' parameter specified is null or undefined!");
                }
                else
                {
                    const char **pstr = va_arg(vl, const char **);
                    if (pstr == NULL)
                    {
                        JSE_ERROR("pstr is NULL")
                        ret = DUK_RET_ERROR;
                        break;
                    }
                    else
                    {
                        *pstr = duk_safe_to_string(ctx, i);
                        if (*pstr)
                        {
                            if (!strlen(*pstr))
                            {
                                /* Does not return */
                                JSE_THROW_TYPE_ERROR(ctx, "%s - parameter %d (any) empty", func, i);
                            }
                        }
                        else
                        {
                            /* Does not return */
                            JSE_THROW_TYPE_ERROR(ctx, "%s - parameter %d (any) missing", func, i);
                        }
                    }
                }
                break;

            default:
                break;
        }
    }

    va_end(vl);

    /* Success and internal errors will get us here. Type errors will have
       been thrown which never return. */
    return ret;
}

/**
 * @brief The binding for getStr()
 *
 * This function calls the real CCSP API:
 *  - CcspBaseIf_getParameterValues()
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t getStr(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    char *dotstr = 0;
    char *ppDestComponentName = NULL;
    char *ppDestPath = NULL;
    int size = 0;
    parameterValStruct_t **parameterVal = NULL;
    char retParamVal[512] = {0};
    int returnStatus = 0;
    char subSystemPrefix[6] = {0};

    JSE_ASSERT(ctx != NULL)
    JSE_ENTER("getStr(%p)", ctx)

    /* Parse Input parameters first */
    if (parse_parameter(__FUNCTION__, ctx, "s", &dotstr) != 0)
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        JSE_VERBOSE("dotstr=\"%s\"", dotstr)

        /* Check whether there are subsystem prefix in the dot string
           Split Subsytem prefix and COSA dotstr if subsystem prefix is found */
        CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix=\"%s\"", subSystemPrefix)

        /* Get Destination component */
        returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);
        if (returnStatus != 0)
        {
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, returnStatus,
                "UiDbusClientGetDestComponent() failed: \"%s\"", dotstr);
        }
        else
        {
            /* Get Parameter Vaues from ccsp */
            returnStatus = CcspBaseIf_getParameterValues(bus_handle,
                                                         ppDestComponentName,
                                                         ppDestPath,
                                                         &dotstr,
                                                         1,
                                                         &size,
                                                         &parameterVal);

            if (CCSP_SUCCESS != returnStatus)
            {
                free_parameterValStruct_t(bus_handle, size, parameterVal);

                /* Does not return */
                JSE_THROW_COSA_ERROR(ctx, returnStatus,
                    "CcspBaseIf_getParameterValues() failed: \"%s\"", dotstr);
            }
            else
            {
                JSE_DEBUG(
                    "parameterVal[0]->parameterValue=\"%s\"",
                    parameterVal[0]->parameterValue)

                if (size >= 1)
                {
                    strncpy(retParamVal, parameterVal[0]->parameterValue, sizeof(retParamVal));
                }

                JSE_VERBOSE("retParamVal=\"%s\"", retParamVal)

                free_parameterValStruct_t(bus_handle, size, parameterVal);

                /* Return only first param value */
                duk_push_string(ctx, retParamVal);

                /* Return one item, the last value in the stack. */
                ret = 1;
            }
        }
    }

    JSE_EXIT("getStr()=%d", ret)
    return ret;
}

/**
 * @brief The binding for setStr()
 *
 * This function calls the real CCSP APIs:
 *  - CcspBaseIf_getParameterValues()
 *  - CcspBaseIf_setParameterValues()
 * It takes the following arguments:
 *  - DM key/parameter
 *  - value to be set
 *  - commit flag
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t setStr(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    char *dotstr = NULL;
    char *val = NULL;
    duk_bool_t bCommit = 0;
    char *ppDestComponentName = NULL;
    char *ppDestPath = NULL;
    int size;
    parameterValStruct_t **structGet = NULL;
    parameterValStruct_t structSet[1];
    int returnStatus;
    char *pFaultParameterNames = NULL;
    char subSystemPrefix[6] = {0};
    dbus_bool bDbusCommit = 1;

    JSE_ASSERT(ctx != NULL)
    JSE_ENTER("setStr(%p)", ctx)

    /* Parse Parameters first */
    if (parse_parameter(__FUNCTION__, ctx, "sab", &dotstr, &val, &bCommit) != 0)
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        char *valcopy = strdup(val);
        if (valcopy == NULL)
        {
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, CCSP_ERR_MEMORY_ALLOC_FAIL,
                "strdup(): %s", strerror(errno));
        }
        else
        {
            JSE_VERBOSE("dotstr=\"%s\",val=\"%s\",bCommit=%s", dotstr, valcopy, bCommit ? "true" : "false")

            bDbusCommit = (bCommit) ? 1 : 0;

            /* Check whether there is subsystem prefix in the dot string
            Split Subsytem prefix and COSA dotstr if subsystem prefix is found */
            CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

            JSE_VERBOSE("subSystemPrefix=\"%s\"", subSystemPrefix)

            /* Get Destination component */
            returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);
            if (returnStatus != 0)
            {
                free(valcopy);

                /* Does not return */
                JSE_THROW_COSA_ERROR(ctx, returnStatus,
                    "UiDbusClientGetDestComponent() failed: \"%s\"", dotstr);
            }
            else
            {
                /* First Get the current parameter Values */
                returnStatus = CcspBaseIf_getParameterValues(bus_handle,
                                                            ppDestComponentName,
                                                            ppDestPath,
                                                            &dotstr,
                                                            1,
                                                            &size,
                                                            &structGet);
                if (returnStatus != CCSP_SUCCESS)
                {
                    free(valcopy);

                    /* Does not return */
                    JSE_THROW_COSA_ERROR(ctx, returnStatus,
                        "CcspBaseIf_getParameterValues() failed: \"%s\"", dotstr);
                }
                else
                {
                    JSE_DEBUG(
                        "structGet[0]->parameterName=\"%s\", structGet[0]->parameterValue=\"%s\"",
                        structGet[0]->parameterName,
                        structGet[0]->parameterValue)

                    if (size != 1 || strcmp(structGet[0]->parameterName, dotstr) != 0)
                    {
                        JSE_ERROR("Miss match!")
                    }
                    else
                    {
                        structSet[0].parameterName = (char *)dotstr;
                        structSet[0].parameterValue = valcopy;
                        structSet[0].type = structGet[0]->type;
                        returnStatus =
                            CcspBaseIf_setParameterValues(
                                bus_handle,
                                ppDestComponentName,
                                ppDestPath,
                                0,
                                CCSP_COMPONENT_ID_WebUI,
                                structSet,
                                1,
                                bDbusCommit,
                                &pFaultParameterNames);

                        /* Per C99 free(NULL) is a NOP */
                        free(pFaultParameterNames);
                        free_parameterValStruct_t(bus_handle, size, structGet);

                        if (CCSP_SUCCESS != returnStatus)
                        {
                            free(valcopy);

                            /* Does not return */
                            JSE_THROW_COSA_ERROR(ctx, returnStatus,
                                "CcspBaseIf_setParameterValues() failed: \"%s\" \"%s\" %d",
                                dotstr,
                                structSet[0].parameterValue,
                                bDbusCommit);
                        }
                        else
                        {
                            JSE_DEBUG(
                                "dotstr=\"%s\", structSet[0].parameterValue=\"%s\", bDbusCommit=%d",
                                dotstr,
                                structSet[0].parameterValue,
                                bDbusCommit);

                            /* Return nothing (undefined) */
                            ret = 0;
                        }
                    }
                }
            }

            free(valcopy);
        }
    }

    JSE_EXIT("setStr()=%d", ret)
    return ret;
}

/**
 * @brief The binding for getInstanceIds()
 *
 * This function calls the real CCSP API:
 *  - CcspBaseIf_GetNextLevelInstances()
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t getInstanceIds(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    char *dotstr = NULL;
    char *ppDestComponentName = NULL;
    char *ppDestPath = NULL;
    int returnStatus;
    unsigned int InstNum = 0;
    unsigned int *pInstNumList = NULL;
    size_t loop1 = 0, loop2 = 0;
    int len = 0;
    char format_s[512] = {0};
    char subSystemPrefix[6] = {0};

    JSE_ASSERT(ctx != NULL)
    JSE_ENTER("getInstanceIds(%p)", ctx)

    if (DUK_EXEC_SUCCESS != parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        JSE_VERBOSE("dotstr=\"%s\"", dotstr)

        /* Check whether there are subsystem prefix in the dot string
           Split Subsytem prefix and COSA dotstr if subsystem prefix is found */
        CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix=\"%s\"", subSystemPrefix)

        /* Get Destination component */
        returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);
        if (returnStatus != 0)
        {
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, returnStatus,
                "UiDbusClientGetDestComponent() failed: \"%s\"", dotstr);
        }
        else
        {
            /* Get Next Instance Numbers */
            returnStatus =
                CcspBaseIf_GetNextLevelInstances(
                    bus_handle,
                    ppDestComponentName,
                    ppDestPath,
                    dotstr,
                    &InstNum,
                    &pInstNumList);

            if (returnStatus != CCSP_SUCCESS)
            {
                /* Does not return */
                JSE_THROW_COSA_ERROR(ctx, returnStatus,
                    "CcspBaseIf_GetNextLevelInstances() failed");
            }
            else
            {
                for (loop1 = 0, loop2 = 0; loop1 < (size_t)(InstNum) && loop2 < sizeof(format_s); loop1++)
                {
                    len = snprintf(&format_s[loop2], sizeof(format_s) - loop2, "%d,", pInstNumList[loop1]);
                    loop2 = loop2 + len;
                }

                duk_push_string(ctx, format_s);

                /* One item returned on the bottom of the stack, the string */
                ret = 1;
            }
            if (pInstNumList)
            {
                free(pInstNumList);
            }
        }
    }

    JSE_EXIT("getInstanceIds()=%d", ret)
    return ret;
}

/**
 * @brief The binding for addTblObj()
 *
 * This function calls the real CCSP API:
 *  - CcspBaseIf_AddTblRow()
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t addTblObj(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    char *dotstr = NULL;
    char *ppDestComponentName = NULL;
    char *ppDestPath = NULL;
    int returnStatus;
    int returnInstNum = 0;
    char subSystemPrefix[6] = {0};

    JSE_ASSERT(ctx != NULL)
    JSE_ENTER("addTblObj(%p)", ctx)

    if (parse_parameter(__FUNCTION__, ctx, "s", &dotstr) != 0)
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        JSE_VERBOSE("dotstr=\"%s\"", dotstr)

        /* Check whether there are subsystem prefix in the dot string
           Split Subsytem prefix and COSA dotstr if subsystem prefix is found */
        CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix=\"%s\"", subSystemPrefix)

        /* Get Destination component */
        returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);
        if (returnStatus != 0)
        {
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, returnStatus,
                "UiDbusClientGetDestComponent() failed: \"%s\"", dotstr);
        }
        else
        {
            returnStatus =
                CcspBaseIf_AddTblRow(
                    bus_handle,
                    ppDestComponentName,
                    ppDestPath,
                    0,
                    dotstr,
                    &returnInstNum);

            if (returnStatus != CCSP_SUCCESS)
            {
                /* Does not return */
                JSE_THROW_COSA_ERROR(ctx, returnStatus,
                    "CcspBaseIf_AddTblRow() failed: \"%s\"", dotstr);
            }
            else
            {
                duk_push_int(ctx, returnInstNum);

                /* One item returned on the bottom of the stack, the instance number */
                ret = 1;
            }
        }
    }

    JSE_EXIT("addTblObj()=%d", ret)
    return ret;
}

/**
 * @brief The binding for delTblObj()
 *
 * This function calls the real CCSP API:
 *  - CcspBaseIf_DeleteTblRow()
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t delTblObj(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    char *dotstr = NULL;
    char *ppDestComponentName = NULL;
    char *ppDestPath = NULL;
    int returnStatus;
    char subSystemPrefix[6] = {0};

    JSE_ASSERT(ctx != NULL)
    JSE_ENTER("delTblObj(%p)", ctx)

    if (parse_parameter(__FUNCTION__, ctx, "s", &dotstr) != 0)
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        JSE_VERBOSE("dotstr=\"%s\"", dotstr)

        /* Check whether there are subsystem prefix in the dot string
           Split Subsytem prefix and COSA dotstr if subsystem prefix is found */
        CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix=\"%s\"", subSystemPrefix)

        // Get Destination component
        returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);
        if (returnStatus != 0)
        {
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, returnStatus,
                "UiDbusClientGetDestComponent() failed: \"%s\"", dotstr);
        }
        else
        {
            returnStatus =
                CcspBaseIf_DeleteTblRow(
                    bus_handle,
                    ppDestComponentName,
                    ppDestPath,
                    0,
                    dotstr);

            if (returnStatus != CCSP_SUCCESS)
            {
                /* Does not return */
                JSE_THROW_COSA_ERROR(ctx, returnStatus,
                    "CcspBaseIf_DeleteTblRow() failed: \"%s\"", dotstr);
            }
            else
            {
                /* Return nothing (undefined) */
                ret = 0;
            }
        }
    }

    JSE_EXIT("delTblObj()=%d", ret)
    return ret;
}

/**
 * @brief The binding for DmExtGetStrsWithRootObj()
 *
 * This function calls the real CCSP API:
 *  - CcspBaseIf_getParameterValues()
 * It takes the following arguments:
 *  - root object name, e.g. Device.NAT.
 *  - array of DM parameter names
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t DmExtGetStrsWithRootObj(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    char *pRootObjName;
    char subSystemPrefix[6] = {0};
    int returnStatus = 0;
    char *pDestComponentName = NULL;
    char *pDestPath = NULL;

    duk_idx_t pParamNameArray;
    int paramCount;
    char **ppParamNameList = NULL;
    int index = 0;
    int valCount = 0;
    parameterValStruct_t **ppParameterVal = NULL;

    JSE_ASSERT(ctx != NULL)
    JSE_ENTER("DmExtGetStrsWithRootObj(%p)", ctx)

    /* Parse paremeters */
    if (parse_parameter(__FUNCTION__, ctx, "so", &pRootObjName, &pParamNameArray) != 0)
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        JSE_VERBOSE("pRootObjName=\"%s\"", pRootObjName)

        /* Check whether there is subsystem prefix in the dot string
        Split Subsytem prefix and COSA dotstr if subsystem prefix is found */
        CheckAndSetSubsystemPrefix(&pRootObjName, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix=\"%s\"", subSystemPrefix)

        /*
        *  Get Destination component for root obj name
        */
        returnStatus = UiDbusClientGetDestComponent(pRootObjName, &pDestComponentName, &pDestPath, subSystemPrefix);
        if (returnStatus != 0)
        {
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, returnStatus,
                "UiDbusClientGetDestComponent() failed: \"%s\"", pRootObjName);
        }
        else
        {
            JSE_DEBUG("pRootObjName=\"%s\", pDestComponentName\"%s\", pDestPath=\"%s\"\n",
                pRootObjName, pDestComponentName, pDestPath)
        }

        /*
        *  Construct parameter name array
        */
        paramCount = get_array_length(ctx, pParamNameArray);

        JSE_VERBOSE("paramCount=%d", paramCount)

        ppParamNameList = (char **)calloc(paramCount, sizeof(char *));
        if (ppParamNameList == NULL)
        {
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, CCSP_ERR_MEMORY_ALLOC_FAIL,
                "calloc() failed: %s", strerror(errno));
        }

        index = 0;

        /* Iterate array and get the values */
        duk_enum(ctx, pParamNameArray, 0);
        while (duk_next(ctx, -1, true /* get val in addition to key */))
        {
            /*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/
            char *value = (char *)duk_get_string(ctx, -1); /*val at -1, key at -2*/
            if (value)
            {
                ppParamNameList[index] = value;
                JSE_VERBOSE("ppParamNameList[%d]=\"%s\"", index, ppParamNameList[index])
                index++;
            }
#ifdef JSE_DEBUG
            else
            {
                if (duk_get_type(ctx, -2) == DUK_TYPE_NUMBER)
                {
                    unsigned int key = duk_get_uint(ctx, -2);
                    JSE_VERBOSE("Item %u is null!", key);
                }
                else if (duk_get_type(ctx, -2) == DUK_TYPE_STRING)
                {
                    const char * key = duk_get_string(ctx, -2);
                    JSE_VERBOSE("Item \"%s\" is null!", key);
                }
                else
                {
                    JSE_VERBOSE("Item with unrecognised key is null!");
                }
            }
#endif
            duk_pop_2(ctx);
        }
        duk_pop(ctx);
        /* done with array */

        returnStatus =
            CcspBaseIf_getParameterValues(
                bus_handle,
                pDestComponentName,
                pDestPath,
                ppParamNameList,
                paramCount,
                &valCount, /* valCount could be larger than paramCount */
                &ppParameterVal);

        if (CCSP_SUCCESS != returnStatus)
        {
            free(ppParamNameList);
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, returnStatus, "CcspBaseIf_getParameterValues() failed");
        }
        else
        {
            /*
            * construct return value array: the first value is return status,
            * the rest are sub arrays, array ( parameter name, value )
            */
            duk_idx_t arr_idx = duk_push_array(ctx);
            duk_push_int(ctx, 0);
            duk_put_prop_index(ctx, arr_idx, 0);

            JSE_VERBOSE("%d values returned:", valCount)

            for (index = 0; index < valCount; index++)
            {
                duk_idx_t sub_idx;
                JSE_VERBOSE(
                    "ppParameterVal[%d]->parameterName=\"%s\", ppParameterVal[%d]->parameterValue=\"%s\"",
                    index, ppParameterVal[index]->parameterName,
                    index, ppParameterVal[index]->parameterValue)

                sub_idx = duk_push_array(ctx);
                duk_push_string(ctx, ppParameterVal[index]->parameterName);
                duk_put_prop_index(ctx, sub_idx, 0);
                duk_push_string(ctx, ppParameterVal[index]->parameterValue);
                duk_put_prop_index(ctx, sub_idx, 1);
                duk_put_prop_index(ctx, arr_idx, index + 1);
            }

            if (valCount > 0)
            {
                free_parameterValStruct_t(bus_handle, valCount, ppParameterVal);
            }

            free(ppParamNameList);

            /* One item returned on the bottom of the stack, the value array */
            ret = 1;
        }
    }

    JSE_EXIT("DmExtGetStrsWithRootObj()=%d", ret)
    return ret;
}

/**
 * @brief The binding for DmExtSetStrsWithRootObj()
 *
 * This function calls the real CCSP API:
 *  - CcspBaseIf_setParameterValues()
 * It takes the following arguments:
 *  - root object name, e.g. Device.NAT.
 *  - commit flag
 *  - array of DM parameter names
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t DmExtSetStrsWithRootObj(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    int returnStatus = CCSP_SUCCESS;
    char *pRootObjName;
    char subSystemPrefix[6] = {0};
    char *pDestComponentName = NULL;
    char *pDestPath = NULL;

    duk_bool_t bCommit = 1;
    dbus_bool bDbusCommit = 1;

    duk_idx_t pParamArray;
    int paramCount;
    parameterValStruct_t *pParameterValList = NULL;
    char BoolStrBuf[16] = {0};
    int index = 0;
    char *pFaultParamName = NULL;

    JSE_ASSERT(ctx != NULL)
    JSE_ENTER("DmExtSetStrsWithRootObj(%p)", ctx)

    /* Parse paremeters */
    if (parse_parameter(__FUNCTION__, ctx, "sbo", &pRootObjName, &bCommit, &pParamArray) != 0)
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        JSE_VERBOSE("pRootObjName=\"%s\", bCommit=%s", pRootObjName, bCommit ? "true" : "false")

        bDbusCommit = (bCommit) ? 1 : 0;

        /* Check whether there is subsystem prefix in the dot string
        Split Subsytem prefix and COSA dotstr if subsystem prefix is found */
        CheckAndSetSubsystemPrefix(&pRootObjName, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix=\"%s\"", subSystemPrefix)

        /*
        *  Get Destination component for root obj name
        */
        returnStatus = UiDbusClientGetDestComponent(pRootObjName, &pDestComponentName, &pDestPath, subSystemPrefix);
        if (returnStatus != 0)
        {
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, returnStatus,
                "UiDbusClientGetDestComponent() failed: \"%s\"", pRootObjName);
        }
        else
        {
            JSE_DEBUG(
                "pRootObjName=\"%s\", bDbusCommit=%d, pDestComponentName=\"%s\", pDestPath=\"%s\"\n",
                pRootObjName,
                bDbusCommit,
                pDestComponentName,
                pDestPath)
        }

        /*
        *  Construct parameter value array
        */
        paramCount = get_array_length(ctx, pParamArray);

        JSE_VERBOSE("paramCount=%d", paramCount)

        pParameterValList = (parameterValStruct_t *)calloc(paramCount, sizeof(parameterValStruct_t));
        if (pParameterValList == NULL)
        {
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, CCSP_ERR_MEMORY_ALLOC_FAIL,
                "calloc() failed: %s", strerror(errno));
        }

        index = 0;

        /*
        *  This is an array of arrays - process the first level
        *  Second level array of each parameter value: parameter name, type and value
        *  Therofore, the count has to be 3
        *  Construct the parameter val struct list for CCSP Base API along the way
        */
        duk_enum(ctx, pParamArray, 0);
        while (duk_next(ctx, -1, 1))
        {
            if (!duk_is_array(ctx, -1))
            {
                free(pParameterValList);

                /* Does not return */
                JSE_THROW_TYPE_ERROR(ctx, "Item is not an array!");
            }
            else if (get_array_length(ctx, -1) != 3)
            {
                free(pParameterValList);

                /* Does not return */
                JSE_THROW_RANGE_ERROR(ctx,
                    "Invalid sub-array size (%d)",
                    get_array_length(ctx, -1));
            }
            else
            {
                duk_enum(ctx, -1, 0);

                if (duk_next(ctx, -1, true)) /* get the name */
                {
                    /*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/
                    pParameterValList[index].parameterName = (char *)duk_get_string(ctx, -1);

                    JSE_VERBOSE("pParameterValList[%d].parameterName=\"%s\"",
                        index, pParameterValList[index].parameterName)

                    /* pop name k/v */
                    duk_pop_2(ctx);

                    if (duk_next(ctx, -1, true)) /* get the type */
                    {
                        /*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/
                        char *pTemp = (char *)duk_get_string(ctx, -1);

                        if (!strcmp(pTemp, "void"))
                        {
                            pParameterValList[index].type = ccsp_none;
                        }
                        else if (!strcmp(pTemp, "string"))
                        {
                            pParameterValList[index].type = ccsp_string;
                        }
                        else if (!strcmp(pTemp, "int"))
                        {
                            pParameterValList[index].type = ccsp_int;
                        }
                        else if (!strcmp(pTemp, "uint"))
                        {
                            pParameterValList[index].type = ccsp_unsignedInt;
                        }
                        else if (!strcmp(pTemp, "bool"))
                        {
                            pParameterValList[index].type = ccsp_boolean;
                        }
                        else if (!strcmp(pTemp, "datetime"))
                        {
                            pParameterValList[index].type = ccsp_dateTime;
                        }
                        else if (!strcmp(pTemp, "base64"))
                        {
                            pParameterValList[index].type = ccsp_base64;
                        }
                        else if (!strcmp(pTemp, "long"))
                        {
                            pParameterValList[index].type = ccsp_long;
                        }
                        else if (!strcmp(pTemp, "unlong"))
                        {
                            pParameterValList[index].type = ccsp_unsignedLong;
                        }
                        else if (!strcmp(pTemp, "float"))
                        {
                            pParameterValList[index].type = ccsp_float;
                        }
                        else if (!strcmp(pTemp, "double"))
                        {
                            pParameterValList[index].type = ccsp_double;
                        }
                        else if (!strcmp(pTemp, "byte"))
                        {
                            pParameterValList[index].type = ccsp_byte;
                        }

                        JSE_VERBOSE("pParameterValList[%d].type=%d, pTemp=\"%s\"",
                            index, pParameterValList[index].type, pTemp)

                        /* pop type k/v */
                        duk_pop_2(ctx);

                        if (duk_next(ctx, -1, true)) /* get the value */
                        {
                            /*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/
                            char * value = (char *)duk_safe_to_string(ctx, -1);

                            if (pParameterValList[index].type == ccsp_boolean)
                            {
                                /* support true/false or 1/0 for boolean value */
                                if (!strcmp(value, "1"))
                                {
                                    strcpy(BoolStrBuf, "true");
                                    value = BoolStrBuf;
                                }
                                else if (!strcmp(value, "0"))
                                {
                                    strcpy(BoolStrBuf, "false");
                                    value = BoolStrBuf;
                                }
                            }

                            JSE_VERBOSE("pParameterValList[%d].parameterValue=\"%s\"\n",
                                index, value)

                            pParameterValList[index].parameterValue = strdup(value);
                            if (pParameterValList[index].parameterValue == NULL)
                            {
                                JSE_ERROR("strdup(): %s", strerror(errno));
                                returnStatus = CCSP_ERR_MEMORY_ALLOC_FAIL;
                            }

                            /* pop value k/v */
                            duk_pop_2(ctx);
                        }
                        else
                        {
                            JSE_ERROR("Subarray invalid")
                            returnStatus = CCSP_FAILURE;
                        }
                    }
                    else
                    {
                        JSE_ERROR("Subarray invalid")
                        returnStatus = CCSP_FAILURE;
                    }
                }
                else
                {
                    JSE_ERROR("Subarray invalid")
                    returnStatus = CCSP_FAILURE;
                }

                index++;

                /* pop sub array */
                duk_pop(ctx);
            }
            /* pop key and value from main array */
            duk_pop_2(ctx);
        }
        /* pop main array */
        duk_pop(ctx);

        if (CCSP_SUCCESS != returnStatus)
        {
            returnStatus =
                CcspBaseIf_setParameterValues(
                    bus_handle,
                    pDestComponentName,
                    pDestPath,
                    0,
                    CCSP_COMPONENT_ID_WebUI,
                    pParameterValList,
                    index, /* use the actual count, instead of paramCount */
                    bDbusCommit,
                    &pFaultParamName);
        }

        for (index = 0; index < paramCount; index++)
        {
            if (pParameterValList[index].parameterValue != NULL)
            {
                free(pParameterValList[index].parameterValue);
            }
        }

        free(pParameterValList);

        if (CCSP_SUCCESS != returnStatus)
        {
            if (pFaultParamName != NULL)
            {
                /* Temporary buffer on the stack which will get cleaned up on throw */
                char faultParamName[256];

                strncpy(faultParamName, pFaultParamName ? pFaultParamName : "none", sizeof(faultParamName) - 1);
                faultParamName[sizeof(faultParamName) - 1] = '\0';

                /* Per C99 free(NULL) is a NOP */
                free(pFaultParamName);

                /* Does not return */
                JSE_THROW_COSA_ERROR(ctx, returnStatus,
                    "CcspBaseIf_setParameterValues() failed: %d: %s",
                    bDbusCommit, faultParamName);
            }
            else
            {
                /* Does not return */
                JSE_THROW_COSA_ERROR(ctx, returnStatus, "Internal error");
            }
        }
        else
        {
            /* Return nothing (undefined) */
            ret = 0;
        }
    }

    JSE_EXIT("DmExtSetStrsWithRootObj()=%d", ret)
    return ret;
}

/**
 * @brief The binding for DmExtGetInstanceIds()
 *
 * This function calls the real CCSP API:
 *  - CcspBaseIf_GetNextLevelInstances()
 * It takes the following argument:
 *  - object table name, e.g. Device.NAT.PortMapping.
 *
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
static duk_ret_t DmExtGetInstanceIds(duk_context *ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    char *dotstr = NULL;
    char subSystemPrefix[6] = {0};
    char *ppDestComponentName = NULL;
    char *ppDestPath = NULL;
    int returnStatus = 0;
    unsigned int InstNum = 0;
    unsigned int *pInstNumList = NULL;
    unsigned int index = 0;

    JSE_ASSERT(ctx != NULL)
    JSE_ENTER("DmExtGetInstanceIds(%p)", ctx)

    if (parse_parameter(__FUNCTION__, ctx, "s", &dotstr) != 0)
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        JSE_VERBOSE("dotstr=\"%s\"", dotstr)

        /* Check whether there are subsystem prefix in the dot string
           Split Subsytem prefix and COSA dotstr if subsystem prefix is found */
        CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix=\"%s\"", subSystemPrefix)

        // Get Destination component
        returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);
        if (returnStatus != 0)
        {
            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, returnStatus,
                "UiDbusClientGetDestComponent() failed: \"%s\"", dotstr);
        }
        else
        {
            JSE_DEBUG("dotstr=\"%s\", ppDestComponentName=\"%s\", ppDestPath=\"%s\"",
                dotstr, ppDestComponentName, ppDestPath)
        }

        /*
        *  Get Next Instance Numbers
        */
        returnStatus =
            CcspBaseIf_GetNextLevelInstances(
                bus_handle,
                ppDestComponentName,
                ppDestPath,
                dotstr,
                &InstNum,
                &pInstNumList);

        if (returnStatus != CCSP_SUCCESS)
        {
            /* Per C99 free(NULL) is a NOP */
            free(pInstNumList);

            /* Does not return */
            JSE_THROW_COSA_ERROR(ctx, returnStatus,
                "CcspBaseIf_GetNextLevelInstances() failed");
        }
        else
        {
            char StrBuf[24];

            duk_idx_t arr_idx = duk_push_array(ctx);
            duk_push_int(ctx, 0);
            duk_put_prop_index(ctx, arr_idx, 0);

            JSE_VERBOSE("InstNum=%d", InstNum)

            for (index = 0; index < InstNum; index++)
            {
                snprintf(StrBuf, sizeof(StrBuf), "%d", pInstNumList[index]);
                duk_push_string(ctx, StrBuf);
                duk_put_prop_index(ctx, arr_idx, index + 1);

                JSE_VERBOSE("Index %d: \"%s\"", index, StrBuf)
            }

            /* Per C99 free(NULL) is a NOP */
            free(pInstNumList);

            /* One item returned on the bottom of the stack, the value array */
            ret = 1;
        }
    }

    JSE_EXIT("DmExtGetInstanceIds()=%d", ret)
    return ret;
}

/* Duktape/C function bind list */
static const duk_function_list_entry ccsp_cosa_funcs[] = {
    {"getStr", getStr, 1},
    {"setStr", setStr, 3},
    {"getInstanceIds", getInstanceIds, 1},
    {"addTblObj", addTblObj, 1},
    {"delTblObj", delTblObj, 1},
    {"DmExtGetStrsWithRootObj", DmExtGetStrsWithRootObj, 2},
    {"DmExtSetStrsWithRootObj", DmExtSetStrsWithRootObj, 3},
    {"DmExtGetInstanceIds", DmExtGetInstanceIds, 1},
    {NULL, NULL, 0}};

/**
 * @brief Bind CCSP functions
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_cosa(jse_context_t* jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_ENTER("jse_bind_cosa(%p)", jse_ctx)

    JSE_VERBOSE("ref_count=%d", ref_count)
    if (jse_ctx != NULL)
    {
        /* jse_cosa is dependent upon cosa error objects so bind here */
        if (jse_bind_cosa_error(jse_ctx) != 0)
        {
            JSE_ERROR("Failed to bind Cosa error objects")
        }
        else
        {
            if (ref_count == 0)
            {
                duk_push_object(jse_ctx->ctx);
                duk_put_function_list(jse_ctx->ctx, -1, ccsp_cosa_funcs);
                duk_put_global_string(jse_ctx->ctx, "Cosa");
            }

            ref_count ++;
            ret = 0;
        }
    }

    JSE_EXIT("jse_bind_cosa()=%d", ret)
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
void jse_unbind_cosa(jse_context_t * jse_ctx)
{
    JSE_ENTER("jse_unbind_cosa(%p)", jse_ctx)

    ref_count --;
    JSE_VERBOSE("ref_count=%d", ref_count)

    /* TODO: Actually unbind */

    jse_unbind_cosa_error(jse_ctx);

    JSE_EXIT("jse_unbind_cosa()")
}
