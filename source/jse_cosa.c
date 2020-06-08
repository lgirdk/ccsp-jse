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
#include <ccsp_message_bus.h>
#include <ccsp_base_api.h>
#include <ccsp_memory.h>
#include <ccsp_custom.h>
#include <duktape.h>

#include "jse_debug.h"
#include "jse_cosa.h"

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
#endif // BUILD_RBUS

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
            "Failed to locate the component for %s%s%s, error code = %d!\n",
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
        strncpy(pSubSystemPrefix, "eRT.", 4);
        *ppDotStr += 4; // shift four bytes to get rid of eRT:
    }
    else if (!strncmp(*ppDotStr, "eMG", 3)) // check wither str has prex of eMG
    {
        strncpy(pSubSystemPrefix, "eMG.", 4);
        *ppDotStr += 4; // shift four bytes to get rid of eMG;
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
        sprintf(dst_pathname_cr, CCSP_DBUS_INTERFACE_CR);
    }
    else
    {
        sprintf(dst_pathname_cr, "eRT." CCSP_DBUS_INTERFACE_CR);
    }

    JSE_VERBOSE("COSA PHP extension starts -- PC sim = %d...\n", gPcSim)

    JSE_VERBOSE("COSA PHP extension RINIT -- initialize dbus...\n")

    returnStatus = CCSP_Message_Bus_Init(COMPONENT_NAME, CONF_FILENAME, &bus_handle, 0, 0);
    if (returnStatus != 0)
    {
        JSE_ERROR("Message bus init failed, error code = %d!\n", returnStatus)
    }

#ifndef BUILD_RBUS
    CCSP_Msg_SleepInMilliSeconds(1000);
    CCSP_Message_Bus_Register_Path(bus_handle, msg_path, path_message_func, 0);
#endif
    return returnStatus;
}

static void cosa_shutdown()
{
    JSE_VERBOSE("COSA PHP extension exits...\n")
#ifndef BUILD_RBUS /*TODO fix ccsp_message_bus.h: it doesn't define CCSP_Message_Bus_Exit if BUILD_RBUS enabled*/
    if (bus_handle)
    {
        CCSP_Message_Bus_Exit(bus_handle);
    }
#endif
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
 * @return an error status or DUK_EXEC_SUCCESS.
 */
static duk_ret_t parse_parameter(const char *func, duk_context *ctx, const char *types, ...)
{
    static const int overrun_guard = 10;
    int i;
    duk_ret_t ret = DUK_RET_ERROR;
    bool success = true;
    va_list vl;

    JSE_ASSERT(func != NULL)
    JSE_ASSERT(ctx != NULL)
    JSE_ASSERT(types != NULL)

    va_start(vl, types);

    for (i = 0; types[i] && i < overrun_guard && success; ++i)
    {
        switch (types[i])
        {
        // string
        case 's':
        {
            if (!duk_is_string(ctx, i))
            {
                JSE_ERROR("Type 's' parameter specified is not a string!")
                (void)duk_type_error(ctx, "Type 's' parameter specified is not a string!");
            }
            else
            {
                const char **pstr = va_arg(vl, const char **);

                JSE_ASSERT(pstr != NULL)

                if (pstr == NULL)
                {
                    success = false;
                }
                else
                {
                    *pstr = duk_get_string(ctx, i);

                    if (*pstr)
                    {
                        if (!strlen(*pstr))
                        {
                            JSE_ERROR("%s - parameter %d (string) empty\n", func, i)
                            success = false;
                        }
                    }
                    else
                    {
                        JSE_ERROR("%s - parameter %d (string) missing\n", func, i)
                        success = false;
                    }
                }
            }
        }
        break;

        // boolean
        case 'b':
        {
            if (!duk_is_boolean(ctx, i))
            {
                JSE_ERROR("Type 'b' parameter specified is not a boolean!")
                (void)duk_type_error(ctx, "Type 'b' parameter specified is not a boolean!");
            }
            else
            {
                duk_bool_t *pbool = va_arg(vl, int *);

                JSE_ASSERT(pbool != NULL)

                if (pbool == NULL)
                {
                    success = false;
                }
                else
                {
                    *pbool = duk_get_boolean(ctx, i);
                }
            }
        }
        break;

        // object, e.g. JS object/array/function or duktape thread/internal object
        case 'o':
        {
            if (!duk_is_object(ctx, i))
            {
                JSE_ERROR("Type 'o' parameter specified is not an object!")
                (void)duk_type_error(ctx, "Type 'o' parameter specified is not an object!");
            }
            else
            {
                duk_idx_t *pidx = va_arg(vl, int *);

                JSE_ASSERT(pidx != NULL)

                if (pidx == NULL)
                {
                    success = false;
                }
                else
                {
                    *pidx = i;
                }
            }
        }
        break;

        // number
        case 'n':
        {
            if (!duk_is_number(ctx, i))
            {
                JSE_ERROR("Type 'n' parameter specified is not a number!")
                (void)duk_type_error(ctx, "Type 'n' parameter specified is not a number!");
            }
            else
            {
                duk_double_t *pdouble = va_arg(vl, double *);

                JSE_ASSERT(pdouble != NULL)

                if (pdouble == NULL)
                {
                    success = false;
                }
                else
                {
                    *pdouble = duk_get_number(ctx, i);
                }
            }
        }
        break;

        default:
        {
        }
        break;
        }
    }

    va_end(vl);

    ret = (success == true) ? DUK_EXEC_SUCCESS : DUK_RET_ERROR;

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

    // Parse Input parameters first
    if (DUK_EXEC_SUCCESS != parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        // Check whether there are subsystem prefix in the dot string
        // Split Subsytem prefix and COSA dotstr if subsystem prefix is found
        CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix = %s", subSystemPrefix)

        // Get Destination component
        returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);
        if (returnStatus != 0)
        {
            JSE_ERROR("Failed on UiDbusClientGetDestComponent %s, error code = %d", dotstr, returnStatus)
        }
        else
        {
            // Get Parameter Vaues from ccsp
            returnStatus = CcspBaseIf_getParameterValues(bus_handle,
                                                         ppDestComponentName,
                                                         ppDestPath,
                                                         &dotstr,
                                                         1,
                                                         &size,
                                                         &parameterVal);

            if (CCSP_SUCCESS != returnStatus)
            {
                JSE_ERROR("Failed on CcspBaseIf_getParameterValues %s, error code = %d.\n", dotstr, returnStatus)
            }
            else
            {
                JSE_DEBUG(
                    "CcspBaseIf_getParameterValues: %s, error code %d, result %s!\n",
                    dotstr,
                    returnStatus,
                    parameterVal[0]->parameterValue)

                if (size >= 1)
                {
                    strncpy(retParamVal, parameterVal[0]->parameterValue, sizeof(retParamVal));
                }
                // Return only first param value
                duk_push_string(ctx, retParamVal);

                ret = 1; // value on stack top is return value
            }
            free_parameterValStruct_t(bus_handle, size, parameterVal);
        }
    }

    JSE_VERBOSE("string=%s, ret=%d", retParamVal, ret)

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

    // Parse Parameters first
    if (DUK_EXEC_SUCCESS != parse_parameter(__FUNCTION__, ctx, "ssb", &dotstr, &val, &bCommit))
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        bDbusCommit = (bCommit) ? 1 : 0;

        // Check whether there is subsystem prefix in the dot string
        // Split Subsytem prefix and COSA dotstr if subsystem prefix is found
        CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix = %s", subSystemPrefix)

        // Get Destination component
        returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);

        if (returnStatus != 0)
        {
            JSE_ERROR("Failed on UiDbusClientGetDestComponent %s, error code = %d", dotstr, returnStatus)
        }
        else
        {
            // First Get the current parameter Values
            returnStatus = CcspBaseIf_getParameterValues(bus_handle,
                                                         ppDestComponentName,
                                                         ppDestPath,
                                                         &dotstr,
                                                         1,
                                                         &size,
                                                         &structGet); //&parameterVal);

            if (returnStatus != CCSP_SUCCESS)
            {
                JSE_ERROR("Failed on CcspBaseIf_getParameterValues %s, error code = %d.\n", dotstr, returnStatus)
            }
            else
            {
                JSE_DEBUG(
                    "setStr - CcspBaseIf_getParameterValues: %s, error code %d, result %s!\n",
                    structGet[0]->parameterName,
                    returnStatus,
                    structGet[0]->parameterValue)

                if (size != 1 || strcmp(structGet[0]->parameterName, dotstr) != 0)
                {
                    JSE_ERROR("%s: miss match", __FUNCTION__)
                }
                else
                {
                    // Its Dangerous to use strcpy() but we dont have any option
                    // strcpy(parameterVal[0]->parameterValue,val);
                    structSet[0].parameterName = (char *)dotstr;
                    structSet[0].parameterValue = val;
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

                    if (CCSP_SUCCESS != returnStatus)
                    {
                        JSE_ERROR(
                            "CcspBaseIf_setParameterValues failed - %s:%s bCommit:%d, error code = %d, fault parameter = %s.\n",
                            dotstr,
                            structSet[0].parameterValue,
                            bDbusCommit,
                            returnStatus,
                            pFaultParameterNames ? pFaultParameterNames : "none")
                    }
                    else
                    {
                        JSE_DEBUG(
                            "CcspBaseIf_setParameterValues - %s:%s bCommit:%d, error code = %d.\n",
                            dotstr,
                            structSet[0].parameterValue,
                            bDbusCommit,
                            returnStatus);

                        ret = 0;
                    }
                    if (pFaultParameterNames)
                        free(pFaultParameterNames);

                    free_parameterValStruct_t(bus_handle, size, structGet);
                }
            }
        }
    }

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
    unsigned int loop1 = 0, loop2 = 0;
    int len = 0;
    char format_s[512] = {0};
    char subSystemPrefix[6] = {0};

    JSE_ASSERT(ctx != NULL)

    if (DUK_EXEC_SUCCESS != parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        // Check whether there are subsystem prefix in the dot string
        // Split Subsytem prefix and COSA dotstr if subsystem prefix is found
        CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix = %s", subSystemPrefix)

        // Get Destination component
        returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);

        if (returnStatus != 0)
        {
            JSE_ERROR("Failed on UiDbusClientGetDestComponent %s, error code = %d", dotstr, returnStatus)
        }
        else
        {
        // Get Next Instance Numbers
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
                JSE_ERROR("Failed on CcspBaseIf_GetNextLevelInstances, error code = %d.\n", returnStatus)
            }
            else
            {
                for (loop1 = 0, loop2 = 0; loop1 < (InstNum); loop1++)
                {
                    len = sprintf(&format_s[loop2], "%d,", pInstNumList[loop1]);
                    loop2 = loop2 + len;
                }

                //Place NULL char at the end of string
                format_s[loop2 - 1] = 0;
                duk_push_string(ctx, format_s);

                ret = 1; // value on stack top is return value
            }
            if (pInstNumList)
            {
                free(pInstNumList);
            }
        }
    }
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

    if (DUK_EXEC_SUCCESS != parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        // Check whether there are subsystem prefix in the dot string
        // Split Subsytem prefix and COSA dotstr if subsystem prefix is found
        CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix = %s", subSystemPrefix)

        // Get Destination component
        returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);

        if (returnStatus != 0)
        {
            JSE_ERROR("Failed on UiDbusClientGetDestComponent %s, error code = %d", dotstr, returnStatus)
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
                JSE_ERROR("addTblObj - CcspBaseIf_AddTblRow failed on %s, error code = %d!\n", dotstr, returnStatus)
            }
            else
            {
                duk_push_int(ctx, returnInstNum);

                ret = 1; // value on stack top is return value
            }
        }
    }
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

    if (DUK_EXEC_SUCCESS != parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        JSE_ERROR("Error parsing argument(s)!")
    }
    else
    {
        // Check whether there are subsystem prefix in the dot string
        // Split Subsytem prefix and COSA dotstr if subsystem prefix is found
        CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

        JSE_VERBOSE("subSystemPrefix = %s", subSystemPrefix)

        // Get Destination component
        returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);

        if (returnStatus != 0)
        {
            JSE_ERROR("Failed on UiDbusClientGetDestComponent %s, error code = %d", dotstr, returnStatus)
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
                JSE_ERROR("delTblObj - CcspBaseIf_DeleteTblRow failed on %s, error code = %d!\n", dotstr, returnStatus)
            }
            else
            {
                returnStatus = 0;

                ret = 1; // value on stack top is return value
            }
        }
    }

    //RETURN_LONG(returnStatus);
    duk_push_int(ctx, returnStatus); // not entirely sure about this return value, but just mimicking current logic for now

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

    /* Parse paremeters */
    if (DUK_EXEC_SUCCESS != parse_parameter(__FUNCTION__, ctx, "so", &pRootObjName, &pParamNameArray))
    {
        JSE_ERROR("Error parsing argument(s)!")
        returnStatus = CCSP_FAILURE;
        goto EXIT0;
    }

    // Check whether there is subsystem prefix in the dot string
    // Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&pRootObjName, subSystemPrefix);

    JSE_VERBOSE("subSystemPrefix = %s", subSystemPrefix)

    /*
     *  Get Destination component for root obj name
     */
    returnStatus = UiDbusClientGetDestComponent(pRootObjName, &pDestComponentName, &pDestPath, subSystemPrefix);

    if (returnStatus != 0)
    {
        JSE_ERROR("Failed on UiDbusClientGetDestComponent %s, error code = %d", pRootObjName, returnStatus)
        goto EXIT0;
    }
    else
    {
        JSE_DEBUG("DmExtGetStrsWithRootObj -- RootObjName: %s, destination component: %s, %s\n", 
            pRootObjName, pDestComponentName, pDestPath)
    }

    /*
     *  Construct parameter name array
     */
    paramCount = get_array_length(ctx, pParamNameArray);

    JSE_VERBOSE("Name list count %d:\n", paramCount)

    ppParamNameList = (char **)malloc(sizeof(char *) * paramCount);

    if (ppParamNameList == NULL)
    {
        JSE_ERROR("Failed to allocate ppParamNameList!\n")
        returnStatus = CCSP_ERR_MEMORY_ALLOC_FAIL;
        goto EXIT0;
    }

    index = 0;

    /* Iterate array and get the values */
    duk_enum(ctx, pParamNameArray, 0);
    while (duk_next(ctx, -1, 1 /* get val in addition to key */))
    {
        char *value = (char *)duk_get_string(ctx, -1); /*val at -1, key at -2*/ /*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/
        if (value)
        {
            ppParamNameList[index] = value;
            JSE_VERBOSE("  %s\n", ppParamNameList[index])
            index++;
        }
        else
        {
            JSE_VERBOSE("  value is null\n")
        }
        duk_pop(ctx); /* pop key */
        duk_pop(ctx); /* pop val */
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
        JSE_ERROR("Failed on CcspBaseIf_getParameterValues, error code = %d.\n", returnStatus)
        goto EXIT1;
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

        JSE_VERBOSE("%d of returned values:\n", valCount)

        for (index = 0; index < valCount; index++)
        {
            duk_idx_t sub_idx;
            JSE_VERBOSE("  %s = %s\n", ppParameterVal[index]->parameterName, ppParameterVal[index]->parameterValue)

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

        returnStatus = 0;
    }

EXIT1:

    if (ppParamNameList)
    {
        free(ppParamNameList);
    }

    if (returnStatus == 0) // success
    {
        return 1;
    }

EXIT0:

    return duk_error(ctx, DUK_ERR_ERROR, "CCSP Error: %d", returnStatus); // never returns, throws exception
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
    char *pRootObjName;
    char subSystemPrefix[6] = {0};
    int returnStatus = 0;
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

    /* Parse paremeters */
    if (DUK_EXEC_SUCCESS != parse_parameter(__FUNCTION__, ctx, "sbo", &pRootObjName, &bCommit, &pParamArray))
    {
        JSE_ERROR("Error parsing argument(s)!")
        returnStatus = CCSP_FAILURE;
        goto EXIT0;
    }

    bDbusCommit = (bCommit) ? 1 : 0;

    // Check whether there is subsystem prefix in the dot string
    // Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&pRootObjName, subSystemPrefix);

    JSE_VERBOSE("subSystemPrefix = %s", subSystemPrefix)

    /*
     *  Get Destination component for root obj name
     */
    returnStatus = UiDbusClientGetDestComponent(pRootObjName, &pDestComponentName, &pDestPath, subSystemPrefix);

    if (returnStatus != 0)
    {
        JSE_ERROR("Failed on UiDbusClientGetDestComponent %s, error code = %d", pRootObjName, returnStatus)
        goto EXIT0;
    }
    else
    {
        JSE_DEBUG(
            "DmExtSetStrsWithRootObj -- RootObjName: %s, bCommit: %d, destination component: %s, %s\n",
            pRootObjName,
            bDbusCommit,
            pDestComponentName,
            pDestPath)
    }

    /*
     *  Construct parameter value array
     */
    paramCount = get_array_length(ctx, pParamArray);

    JSE_VERBOSE("Parameter list count: %d", paramCount)

    pParameterValList = (parameterValStruct_t *)malloc(sizeof(parameterValStruct_t) * paramCount);

    if (pParameterValList == NULL)
    {
        JSE_ERROR("Failed to allocate pParameterValList!")
        goto EXIT0;
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
            JSE_ERROR("Item is not ARRAY!")
            (void)duk_type_error(ctx, "Item is not ARRAY!");
        }
        else if (get_array_length(ctx, -1) != 3)
        {
            JSE_ERROR("Subarray count is supposed to be 3, actual value = %d!!!", get_array_length(ctx, -1))
            (void)duk_range_error(ctx, "Subarray count is supposed to be 3, actual value = %d!!!", get_array_length(ctx, -1));
        }
        else
        {
            duk_enum(ctx, -1, 0);

            if (duk_next(ctx, -1, 1)) /* get the name */
            {
                pParameterValList[index].parameterName = (char *)duk_get_string(ctx, -1); /*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/
                JSE_VERBOSE("  Param name %s", pParameterValList[index].parameterName)
                /* pop name k/v */
                duk_pop(ctx);
                duk_pop(ctx);

                if (duk_next(ctx, -1, 1)) /* get the type */
                {
                    char *pTemp = (char *)duk_get_string(ctx, -1); /*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/

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

                    JSE_VERBOSE("  Param type %d->%s", pParameterValList[index].type, pTemp)
                    /* pop type k/v */
                    duk_pop(ctx);
                    duk_pop(ctx);

                    if (duk_next(ctx, -1, 1)) /* get the value */
                    {
                        pParameterValList[index].parameterValue = (char *)duk_get_string(ctx, -1); /*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/

                        if (pParameterValList[index].type == ccsp_boolean)
                        {
                            /* support true/false or 1/0 for boolean value */
                            if (!strcmp(pParameterValList[index].parameterValue, "1"))
                            {
                                strcpy(BoolStrBuf, "true");
                                pParameterValList[index].parameterValue = BoolStrBuf;
                            }
                            else if (!strcmp(pParameterValList[index].parameterValue, "0"))
                            {
                                strcpy(BoolStrBuf, "false");
                                pParameterValList[index].parameterValue = BoolStrBuf;
                            }
                        }
                        JSE_VERBOSE("  Param Value %s\n", pParameterValList[index].parameterValue)
                        /* pop value k/v */
                        duk_pop(ctx);
                        duk_pop(ctx);
                    }
                    else
                    {
                        JSE_WARNING("Subarray invalid")
                    }
                }
                else
                {
                    JSE_WARNING("Subarray invalid")
                }
            }
            else
            {
                JSE_WARNING("Subarray invalid")
            }

            index++;

            /* pop sub array */
            duk_pop(ctx);
        }
        /* pop key and value from main array */
        duk_pop(ctx);
        duk_pop(ctx);
    }
    /* pop main array */
    duk_pop(ctx);

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

    if (CCSP_SUCCESS != returnStatus)
    {
        JSE_ERROR(
            "CcspBaseIf_setParameterValues failed, bCommit:%d, error code = %d, fault parameter = %s",
            bDbusCommit,
            returnStatus,
            pFaultParamName ? pFaultParamName : "none")

        if (pFaultParamName)
        {
            free(pFaultParamName);
        }

        goto EXIT1;
    }
    else
    {
        JSE_DEBUG("CcspBaseIf_setParameterValues succeeded!\n")
        returnStatus = 0;
    }

EXIT1:

    if (pParameterValList)
    {
        free(pParameterValList);
    }

    if (returnStatus == 0) // success
    {
        return 1;
    }

EXIT0:

    return duk_error(ctx, DUK_ERR_ERROR, "CCSP Error: %d", returnStatus); // never returns, throws exception
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
    char *dotstr = NULL;
    char subSystemPrefix[6] = {0};
    char *ppDestComponentName = NULL;
    char *ppDestPath = NULL;
    int returnStatus = 0;
    unsigned int InstNum = 0;
    unsigned int *pInstNumList = NULL;
    unsigned int index = 0;

    JSE_ASSERT(ctx != NULL)

    if (DUK_EXEC_SUCCESS != parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        JSE_ERROR("Error parsing argument(s)!")
        returnStatus = CCSP_FAILURE;
        goto EXIT0;
    }

    // Check whether there are subsystem prefix in the dot string
    // Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&dotstr, subSystemPrefix);

    JSE_VERBOSE("subSystemPrefix = %s", subSystemPrefix)

    // Get Destination component
    returnStatus = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath, subSystemPrefix);

    if (returnStatus != 0)
    {
        JSE_ERROR("Failed on UiDbusClientGetDestComponent %s, error code = %d", dotstr, returnStatus)
        goto EXIT0;
    }
    else
    {
        JSE_DEBUG("DmExtGetInstanceIds -- ObjectTable: %s, destination component: %s, %s", dotstr, ppDestComponentName, ppDestPath)
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
        JSE_ERROR("Failed on CcspBaseIf_GetNextLevelInstances, error code = %d", returnStatus)
        goto EXIT1;
    }
    else
    {
        char StrBuf[24];

        duk_idx_t arr_idx = duk_push_array(ctx);
        duk_push_int(ctx, 0);
        duk_put_prop_index(ctx, arr_idx, 0);

        JSE_VERBOSE("%d of returned values:", InstNum)

        for (index = 0; index < InstNum; index++)
        {
            snprintf(StrBuf, sizeof(StrBuf) - 1, "%d", pInstNumList[index]);
            duk_push_string(ctx, StrBuf);
            duk_put_prop_index(ctx, arr_idx, index + 1);
            JSE_VERBOSE("Instance %d: %s", index, StrBuf)
        }

        if (pInstNumList)
        {
            free(pInstNumList);
        }

        returnStatus = 0;
    }

EXIT1:

    if (returnStatus == 0) // success
    {
        return 1;
    }

EXIT0:

    return duk_error(ctx, DUK_ERR_ERROR, "CCSP Error: %d", returnStatus); // never returns, throws exception
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
 * @param ctx the duktape context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_cosa(duk_context *ctx)
{
    JSE_ASSERT(ctx != NULL)

    duk_push_object(ctx);
    duk_put_function_list(ctx, -1, ccsp_cosa_funcs);

    return 0;
}
