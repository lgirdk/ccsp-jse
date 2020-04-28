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
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <ccsp_message_bus.h>
#include <ccsp_base_api.h>
#include <ccsp_memory.h>
#include <ccsp_custom.h>
#ifdef COMCAST_SSO
#include "sso_api.h"
#endif /* COMCAST_SSO */
#include "jse_internal.h"

#define COMPONENT_NAME              "ccsp.phpextension"
#define CONF_FILENAME               "/tmp/ccsp_msg.cfg"
#define CCSP_COMPONENT_ID_WebUI     0x00000001
#define COSA_PHP_EXT_PCSIM          "/tmp/cosa_php_pcsim"

void * bus_handle                 = NULL;
char * dst_componentid            = NULL;
char * dst_pathname               = NULL;
char dst_pathname_cr[64]          = {0};
static int gPcSim                 = 0;

#ifndef BUILD_RBUS /*FIXME: mrollins: completely removed the functionality when rbus enabled -- do we need to add it back with rbus ? */
static const char* msg_path       = "/com/cisco/spvtg/ccsp/phpext" ;
static const char* msg_interface  = "com.cisco.spvtg.ccsp.phpext" ;
static const char* msg_method     = "__send" ;
static const char* Introspect_msg = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
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
                                    "</node>\n"
                                    ;

DBusHandlerResult path_message_func(DBusConnection* conn, DBusMessage* message, void* user_data)
{
    const char *interface = dbus_message_get_interface(message);
    const char *method   = dbus_message_get_member(message);
    DBusMessage *reply;
    //char tmp[4098];
    char *resp = "888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888";
    char *from = 0;
    char *req = 0;
    char * err_msg  = DBUS_ERROR_NOT_SUPPORTED;

    reply = dbus_message_new_method_return (message);
    if (reply == NULL)
    {
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if(!strcmp("org.freedesktop.DBus.Introspectable", interface)  && !strcmp(method, "Introspect"))
    {
        if ( !dbus_message_append_args (reply, DBUS_TYPE_STRING, &Introspect_msg, DBUS_TYPE_INVALID))

        if (!dbus_connection_send (conn, reply, NULL))

        dbus_message_unref (reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }


    if (!strcmp(msg_interface, interface) && !strcmp(method, msg_method))
    {

        if(dbus_message_get_args (message,
                                NULL,
                                DBUS_TYPE_STRING, &from,
                                DBUS_TYPE_STRING, &req,
                                DBUS_TYPE_INVALID))
        {
            dbus_message_append_args (reply, DBUS_TYPE_STRING, &resp, DBUS_TYPE_INVALID);
            if (!dbus_connection_send (conn, reply, NULL))
                dbus_message_unref (reply);
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }
    dbus_message_set_error_name (reply, err_msg) ;
    dbus_connection_send (conn, reply, NULL);
    dbus_message_unref (reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}
#endif

int UiDbusClientGetDestComponent(char* pObjName,char** ppDestComponentName, char** ppDestPath, char* pSystemPrefix)
{
    int                         ret;
    int                         size = 0;
    componentStruct_t **        ppComponents = NULL;

    ret =
        CcspBaseIf_discComponentSupportingNamespace
            (
                bus_handle,
                dst_pathname_cr,
                pObjName,
                pSystemPrefix,
                &ppComponents,
                &size
            );

    if ( ret == CCSP_SUCCESS )
    {
        *ppDestComponentName = ppComponents[0]->componentName;
        ppComponents[0]->componentName = NULL;
        *ppDestPath    = ppComponents[0]->dbusPath;
        ppComponents[0]->dbusPath = NULL;

        while( size )
        {
            if (ppComponents[size-1]->remoteCR_dbus_path)
            {
                free(ppComponents[size-1]->remoteCR_dbus_path);
            }
            if (ppComponents[size-1]->remoteCR_name)
            {
                free(ppComponents[size-1]->remoteCR_name);
            }
            free(ppComponents[size-1]);
            size--;
        }
        return  0;
    }
    else
    {
        CosaPhpExtLog
            (
                "Failed to locate the component for %s%s%s, error code = %d!\n",
                pSystemPrefix,
                strlen(pSystemPrefix) ? "." : "",
                pObjName,
                ret
            );
        return  ret;
    }
}

void CheckAndSetSubsystemPrefix (char** ppDotStr, char* pSubSystemPrefix)
{
    if (!strncmp(*ppDotStr,"eRT",3))   /*check whether str has prex of eRT*/
    {
        strncpy(pSubSystemPrefix,"eRT.",4);
        *ppDotStr +=4;              //shift four bytes to get rid of eRT:
    }
    else if (!strncmp(*ppDotStr,"eMG",3))  //check wither str has prex of eMG
    {
        strncpy(pSubSystemPrefix,"eMG.",4);
        *ppDotStr +=4;  //shit four bytes to get rid of eMG;
    }
}

int cosa_init()
{
  FILE *fp = NULL;
  int iReturnStatus = 0;
  
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
  if ( gPcSim )
  {
    sprintf(dst_pathname_cr, CCSP_DBUS_INTERFACE_CR);
  }
  else
  {
    sprintf(dst_pathname_cr, "eRT." CCSP_DBUS_INTERFACE_CR);
  }

  CosaPhpExtLog("COSA PHP extension starts -- PC sim = %d...\n", gPcSim);

  CosaPhpExtLog("COSA PHP extension RINIT -- initialize dbus...\n");

  iReturnStatus = CCSP_Message_Bus_Init(COMPONENT_NAME, CONF_FILENAME, &bus_handle, 0,0);
  if ( iReturnStatus != 0 )
  {
    CosaPhpExtLog("Message bus init failed, error code = %d!\n", iReturnStatus);
  }
  
#ifndef BUILD_RBUS
  CCSP_Msg_SleepInMilliSeconds(1000);
  CCSP_Message_Bus_Register_Path(bus_handle, msg_path, path_message_func, 0);
#endif
  return 1;
}

void cosa_shutdown()
{
    CosaPhpExtLog("COSA PHP extension exits...\n");
#ifndef BUILD_RBUS /*TODO fix ccsp_message_bus.h: it doesn't define CCSP_Message_Bus_Exit if BUILD_RBUS enabled*/
    if (bus_handle)
      CCSP_Message_Bus_Exit(bus_handle); 
#endif
}

int get_array_length(duk_context *ctx, duk_idx_t i)
{
  int len = 0;
  duk_enum(ctx, i, 0);
  while (duk_next(ctx, -1, 0 )) {
    len++;
    duk_pop(ctx);
  }
  duk_pop(ctx);
  return len;
}

#define RETURN_LONG(res) { duk_push_number(ctx, res); return 1; }
#define RETURN_STRING(res) { duk_push_string(ctx, res); return 1; }
#define RETURN_TRUE { duk_push_true(ctx); return 1; }
#define RETURN_FALSE { duk_push_false(ctx); return 1; }

static duk_ret_t getStr(duk_context *ctx)
{
    char*                   dotstr              = 0;
    char*                   ppDestComponentName = NULL;
    char*                   ppDestPath          = NULL;
    int                     size                = 0;
    parameterValStruct_t ** parameterVal        = NULL;
    char                    retParamVal[1024]   = {0};
    int                     iReturn             = 0;
    char                    subSystemPrefix[6]  = {0};

    //Parse Input parameters first
    if (!parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        RETURN_STRING("");
    }

    //check whether there are subsystem prefix in the dot string
    //Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&dotstr,subSystemPrefix); 

    //Get Destination component 
    iReturn = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath,subSystemPrefix);
    if ( iReturn != 0 )
    {
        RETURN_STRING("");
    }
        
    //Get Parameter Vaues from ccsp
    iReturn = CcspBaseIf_getParameterValues(bus_handle,
                                            ppDestComponentName,
                                            ppDestPath,
                                            &dotstr,
                                            1,
                                            &size ,
                                            &parameterVal);

    if (CCSP_SUCCESS != iReturn)
    {
        CosaPhpExtLog("Failed on CcspBaseIf_getParameterValues %s, error code = %d.\n", dotstr, iReturn);
        //RETURN_STRING("ERROR: Failed on CcspBaseIf_getParameterValues",1);
        RETURN_STRING("");
    }

    CosaPhpExtLog
        (
            "CcspBaseIf_getParameterValues: %s, error code %d, result %s!\n",
            dotstr,
            iReturn,
            parameterVal[0]->parameterValue
        );

    if ( size >= 1 )
    {
        strncpy(retParamVal,parameterVal[0]->parameterValue, sizeof(retParamVal));
        free_parameterValStruct_t(bus_handle, size, parameterVal);
    }

    //Return only first param value
    RETURN_STRING(retParamVal);
}

static duk_ret_t setStr(duk_context *ctx)
{
    char*                         dotstr                = NULL;
    char*                         val                   = NULL;
    duk_bool_t                    bCommit               = 0;
    char*                         ppDestComponentName   = NULL;
    char*                         ppDestPath            = NULL;
    int                           size;
    parameterValStruct_t          **structGet           = NULL;
    parameterValStruct_t          structSet[1];
    int                           iReturn;
    char*                         pFaultParameterNames  = NULL;
    char                          subSystemPrefix[6]    = {0};
    dbus_bool                     bDbusCommit           = 1;

    //Parse Parameters first
    if (!parse_parameter(__FUNCTION__, ctx, "ssb", &dotstr, &val, &bCommit))
    {
        RETURN_FALSE;
    }
    bDbusCommit = (bCommit) ? 1 : 0;

    //check whether there is subsystem prefix in the dot string
    //Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&dotstr,subSystemPrefix); 

    //Get Destination component 
    iReturn = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath,subSystemPrefix);

    if ( iReturn != 0 )
    {
        RETURN_FALSE;
    }

    //First Get the current parameter Vaues 
    iReturn =CcspBaseIf_getParameterValues(bus_handle,
                                            ppDestComponentName,
                                            ppDestPath,
                                            &dotstr,
                                            1,
                                            &size ,
                                            &structGet);//&parameterVal);

    if ( iReturn != CCSP_SUCCESS ) 
    {
        CosaPhpExtLog("Failed on CcspBaseIf_getParameterValues %s, error code = %d.\n", dotstr, iReturn);
        //RETURN_STRING("ERROR: Failed on CcspBaseIf_getParameterValues",1);
        RETURN_FALSE;   
    }

    CosaPhpExtLog
        (
            "setStr - CcspBaseIf_getParameterValues: %s, error code %d, result %s!\n",
            structGet[0]->parameterName,
            iReturn,
            structGet[0]->parameterValue
        );

    if (size != 1 || strcmp(structGet[0]->parameterName, dotstr) != 0)
    {
    #if _DEBUG_
        syslog(LOG_ERR, "%s: miss match", __FUNCTION__);
    #endif
        free_parameterValStruct_t(bus_handle, size, structGet);
        RETURN_FALSE;   
    }

    /*
     *  Remove the following as it has led to unexpected behavior - 
     *      bCommit = True without value change didn't take effect
     *
    //if the value is not changed, we don't need to save the setting
    if(0 == strcmp(structGet[0]->parameterValue, val))
    {
      RETURN_TRUE;
    }
     */

    //Its Dangerous to use strcpy() but we dont have any option
    //strcpy(parameterVal[0]->parameterValue,val);
    structSet[0].parameterName = (char *)dotstr;
    structSet[0].parameterValue = val;
    structSet[0].type = structGet[0]->type;
    iReturn = 
        CcspBaseIf_setParameterValues
            (
                bus_handle,
                ppDestComponentName,
                ppDestPath,
                0,
                CCSP_COMPONENT_ID_WebUI,
                structSet,
                1,
                bDbusCommit,
                &pFaultParameterNames
            );

    if (CCSP_SUCCESS != iReturn) 
    {
        CosaPhpExtLog
            (
                "CcspBaseIf_setParameterValues failed - %s:%s bCommit:%d, error code = %d, fault parameter = %s.\n",
                dotstr,
                structSet[0].parameterValue,
                bDbusCommit,
                iReturn,
                pFaultParameterNames ? pFaultParameterNames : "none"
            );
            
        if (pFaultParameterNames) free(pFaultParameterNames);

        free_parameterValStruct_t(bus_handle, size, structGet);
        RETURN_FALSE;
    }
    
    CosaPhpExtLog
        (
            "CcspBaseIf_setParameterValues - %s:%s bCommit:%d, error code = %d.\n",
            dotstr,
            structSet[0].parameterValue,
            bDbusCommit,
            iReturn
        );

    if(size >= 1)
    {
        free_parameterValStruct_t(bus_handle, size, structGet);
    }

    //RETURN_STRING("", 1);
    RETURN_TRUE;
}

static duk_ret_t getInstanceIds(duk_context *ctx)
{
    char*                           dotstr = NULL;
    char*                           ppDestComponentName = NULL;
    char*                           ppDestPath = NULL ;
    int                             iReturn;
    unsigned int                    InstNum         = 0;
    unsigned int*                   pInstNumList    = NULL;
    unsigned int                    loop1= 0,loop2=0;
    int                             len              = 0;
    char                            format_s[512] ={0};
    char                            subSystemPrefix[6] = {0};

    if (!parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        RETURN_STRING("");
    }

    //check whether there are subsystem prefix in the dot string
    //Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&dotstr,subSystemPrefix); 

    //Get Destination component 
    iReturn = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath,subSystemPrefix);

    if ( iReturn != 0 )
    {
        RETURN_STRING("");
    }

    /*
     *  Get Next Instance Numbes
     */
    iReturn =
        CcspBaseIf_GetNextLevelInstances
            (
                bus_handle,
                ppDestComponentName,
                ppDestPath,
                dotstr,
                &InstNum,
                &pInstNumList
            );

    if (iReturn != CCSP_SUCCESS)
    {
        //AnscTraceWarning("Failed on CcspBaseIf_GetNextLevelInstances, error code = %d.\n", iReturn);
        //RETURN_STRING("ERROR: Failed on CcspBaseIf_GetNextLevelInstances",1);
        RETURN_STRING("");
    }

    for(loop1=0,loop2=0; loop1<(InstNum); loop1++) 
    {
        len =sprintf(&format_s[loop2],"%d,", pInstNumList[loop1]);
        loop2=loop2+len;
    }

    if (pInstNumList)
    {
        free(pInstNumList);
    }

    //Place NULL char at the end of string
    format_s[loop2-1]=0;
    RETURN_STRING(format_s);
}

static duk_ret_t addTblObj(duk_context *ctx)
{
    char*                           dotstr = NULL;
    char*                           ppDestComponentName = NULL;
    char*                           ppDestPath = NULL ;
    int                             iReturn;
    int                             iReturnInstNum = 0;
    char                            subSystemPrefix[6] = {0};

    if (!parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        iReturn = CCSP_FAILURE;
        RETURN_LONG(iReturn);
    }

    //check whether there are subsystem prefix in the dot string
    //Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&dotstr,subSystemPrefix); 

    //Get Destination component 
    iReturn = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath,subSystemPrefix);

    if(iReturn != 0)
    {
        RETURN_LONG(iReturn);
    }

    iReturn =
        CcspBaseIf_AddTblRow
            (
                bus_handle,
                ppDestComponentName,
                ppDestPath,
                0,
                dotstr,
                &iReturnInstNum
            );
    
    if ( iReturn != CCSP_SUCCESS )
    {
        CosaPhpExtLog("addTblObj - CcspBaseIf_AddTblRow failed on %s, error code = %d!\n", dotstr, iReturn);
        RETURN_LONG(iReturn);
    }
    else
    {
        RETURN_LONG(iReturnInstNum);
    }
}

static duk_ret_t delTblObj(duk_context *ctx)
{
    char*                           dotstr = NULL;
    char*                           ppDestComponentName = NULL;
    char*                           ppDestPath = NULL ;
    int                             iReturn;
    char                            subSystemPrefix[6] = {0};

    if (!parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        iReturn = CCSP_FAILURE;
        RETURN_LONG(iReturn);
    }

    //check whether there are subsystem prefix in the dot string
    //Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&dotstr,subSystemPrefix); 

    //Get Destination component 
    iReturn = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath,subSystemPrefix);

    if(iReturn != 0)
    {
        RETURN_LONG(iReturn);
    }

    iReturn =
        CcspBaseIf_DeleteTblRow
            (
                bus_handle,
                ppDestComponentName,
                ppDestPath,
                0,
                dotstr
            );
                    
    if ( iReturn != CCSP_SUCCESS )
    {
        CosaPhpExtLog("delTblObj - CcspBaseIf_DeleteTblRow failed on %s, error code = %d!\n", dotstr, iReturn);
    }
    else
    {
        iReturn = 0;
    }

    RETURN_LONG(iReturn);
}

static duk_ret_t DmExtGetStrsWithRootObj(duk_context *ctx)
{
    char*                           pRootObjName;
    char                            subSystemPrefix[6]  = {0};
    int                             iReturn             = 0;
    char*                           pDestComponentName  = NULL;
    char*                           pDestPath           = NULL;

    duk_idx_t                       pParamNameArray;
    int                             iParamCount;
    char**                          ppParamNameList     = NULL;
    int                             iIndex              = 0;
    int                             iValCount           = 0;
    parameterValStruct_t **         ppParameterVal      = NULL;

    /* Parse paremeters */
    if (!parse_parameter(__FUNCTION__, ctx, "so", &pRootObjName, &pParamNameArray))
    {
        iReturn = CCSP_FAILURE;
        goto  EXIT0;
    }

    //check whether there is subsystem prefix in the dot string
    //Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&pRootObjName,subSystemPrefix); 

    /*
     *  Get Destination component for root obj name
     */
    iReturn = UiDbusClientGetDestComponent(pRootObjName, &pDestComponentName, &pDestPath, subSystemPrefix);

    if ( iReturn != 0 )
    {
        goto  EXIT0;
    }
    else
    {
        CosaPhpExtLog("DmExtGetStrsWithRootObj -- RootObjName: %s, destination component: %s, %s\n", pRootObjName, pDestComponentName, pDestPath);
    }

    /*
     *  Construct parameter name array
     */
    iParamCount = get_array_length(ctx, pParamNameArray);

    CosaPhpExtLog("Name list count %d:\n", iParamCount);

    ppParamNameList = (char**)malloc(sizeof(char*) * iParamCount);
    
    if ( ppParamNameList == NULL )
    {
        CosaPhpExtLog("Failed to allocate ppParamNameList!\n");
        iReturn = CCSP_ERR_MEMORY_ALLOC_FAIL;
        goto  EXIT0;
    }

    iIndex = 0;

    /*iteratate of array and get the values*/
    duk_enum(ctx, pParamNameArray, 0);
    while (duk_next(ctx, -1, 1/*get val in addition to key*/ )) 
    {
      char* value = (char*)duk_get_string(ctx, -1);/*val at -1, key at -2*/ /*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/
      if(value)
      {
        ppParamNameList[iIndex] = value;
        CosaPhpExtLog("  %s\n", ppParamNameList[iIndex]);
        iIndex++;
      }
      else
      {
        CosaPhpExtLog("  value is null\n");
      }
      duk_pop(ctx);/*pop key*/
      duk_pop(ctx);/*pop val*/
    }
    duk_pop(ctx);
    /*done with array*/

    iReturn = 
        CcspBaseIf_getParameterValues
            (
                bus_handle,
                pDestComponentName,
                pDestPath,
                ppParamNameList,
                iParamCount,
                &iValCount,     /* iValCount could be larger than iParamCount */
                &ppParameterVal
            );

    if ( CCSP_SUCCESS != iReturn )
    {
        CosaPhpExtLog("Failed on CcspBaseIf_getParameterValues, error code = %d.\n", iReturn);
        goto  EXIT1;
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

        CosaPhpExtLog("%d of returned values:\n", iValCount);

        for ( iIndex = 0; iIndex < iValCount; iIndex++ )
        {
            duk_idx_t sub_idx;
            CosaPhpExtLog("  %s = %s\n", ppParameterVal[iIndex]->parameterName, ppParameterVal[iIndex]->parameterValue);

            sub_idx = duk_push_array(ctx);
            duk_push_string(ctx, ppParameterVal[iIndex]->parameterName);
            duk_put_prop_index(ctx, sub_idx, 0);
            duk_push_string(ctx, ppParameterVal[iIndex]->parameterValue);
            duk_put_prop_index(ctx, sub_idx, 1);
            duk_put_prop_index(ctx, arr_idx, iIndex+1);
        }

        if ( iValCount > 0 )
        {
            free_parameterValStruct_t(bus_handle, iValCount, ppParameterVal);
        }

        iReturn = 0;
    }

EXIT1:

    if ( ppParamNameList )
    {
        free(ppParamNameList);
    }
    
    if ( iReturn == 0 )
    {
        return 1;
    }

EXIT0:

    /*
     * construct return value array
     */
    {
      duk_idx_t arr_idx = duk_push_array(ctx);
      duk_push_int(ctx, iReturn);
      duk_put_prop_index(ctx, arr_idx, 0);
    }

    return 1;
}

static duk_ret_t DmExtSetStrsWithRootObj(duk_context *ctx)
{
    char*                           pRootObjName;
    char                            subSystemPrefix[6]  = {0};
    int                             iReturn             = 0;
    char*                           pDestComponentName  = NULL;
    char*                           pDestPath           = NULL;

    duk_bool_t                      bCommit             = 1;
    dbus_bool                       bDbusCommit         = 1;

    duk_idx_t                       pParamArray;
    int                             iParamCount;
    parameterValStruct_t *          pParameterValList   = NULL;
    char                            BoolStrBuf[16]      = {0};
    int                             iIndex              = 0;
    char*                           pFaultParamName     = NULL;

    /* Parse paremeters */
    if (!parse_parameter(__FUNCTION__, ctx, "sbo", &pRootObjName, &bCommit, &pParamArray))
    {
        iReturn = CCSP_FAILURE;
        goto  EXIT0;
    }

    bDbusCommit = (bCommit) ? 1 : 0;

    //check whether there is subsystem prefix in the dot string
    //Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&pRootObjName,subSystemPrefix); 

    /*
     *  Get Destination component for root obj name
     */
    iReturn = UiDbusClientGetDestComponent(pRootObjName, &pDestComponentName, &pDestPath, subSystemPrefix);

    if ( iReturn != 0 )
    {
        goto  EXIT0;
    }
    else
    {
        CosaPhpExtLog
            (
                "DmExtSetStrsWithRootObj -- RootObjName: %s, bCommit: %d, destination component: %s, %s\n",
                pRootObjName,
                bDbusCommit,
                pDestComponentName,
                pDestPath
            );
    }

    /*
     *  Construct parameter value array
     */
    iParamCount = get_array_length(ctx, pParamArray);

    CosaPhpExtLog("Parameter list count %d:\n", iParamCount);
    
    pParameterValList = (parameterValStruct_t*)malloc(sizeof(parameterValStruct_t) * iParamCount);
    
    if ( pParameterValList == NULL )
    {
        CosaPhpExtLog("Failed to allocate pParameterValList!\n");
        iReturn = CCSP_ERR_MEMORY_ALLOC_FAIL;
        goto  EXIT0;
    }

    iIndex = 0;

    /*  
     *  This is an array of arrays - process the first level
     *  Second level array of each parameter value: parameter name, type and value
     *  Therofore, the count has to be 3
     *  Construct the parameter val struct list for CCSP Base API along the way
     */
    duk_enum(ctx, pParamArray, 0);
    while (duk_next(ctx, -1, 1)) 
    {
        if(!duk_is_array(ctx, -1))
        {
            CosaPhpExtLog("Item is not ARRAY!\n");
        }
        else if ( get_array_length(ctx, -1) != 3 )
        {
            CosaPhpExtLog("Subarray count is supposed to be 3, actual value = %d!!!\n", get_array_length(ctx, -1));
        }
        else
        {
            duk_enum(ctx, -1, 0);

            if (duk_next(ctx, -1, 1))/*get the name*/
            {
                pParameterValList[iIndex].parameterName = (char*)duk_get_string(ctx, -1);/*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/
                CosaPhpExtLog("  Param name %s\n", pParameterValList[iIndex].parameterName);
                /*pop name k/v*/
                duk_pop(ctx);
                duk_pop(ctx);

                if (duk_next(ctx, -1, 1))/*get the type*/
                {
                    char* pTemp = (char*)duk_get_string(ctx, -1);/*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/

                    if ( !strcmp(pTemp, "void") )
                        pParameterValList[iIndex].type = ccsp_none;
                    else if ( !strcmp(pTemp, "string") )
                        pParameterValList[iIndex].type = ccsp_string;
                    else if ( !strcmp(pTemp, "int") )
                        pParameterValList[iIndex].type = ccsp_int;
                    else if ( !strcmp(pTemp, "uint") )
                        pParameterValList[iIndex].type = ccsp_unsignedInt;
                    else if ( !strcmp(pTemp, "bool") )
                        pParameterValList[iIndex].type = ccsp_boolean;
                    else if ( !strcmp(pTemp, "datetime") )
                        pParameterValList[iIndex].type = ccsp_dateTime;
                    else if ( !strcmp(pTemp, "base64") )
                        pParameterValList[iIndex].type = ccsp_base64;
                    else if ( !strcmp(pTemp, "long") )
                        pParameterValList[iIndex].type = ccsp_long;
                    else if ( !strcmp(pTemp, "unlong") )
                        pParameterValList[iIndex].type = ccsp_unsignedLong;
                    else if ( !strcmp(pTemp, "float") )
                        pParameterValList[iIndex].type = ccsp_float;
                    else if ( !strcmp(pTemp, "double") )
                        pParameterValList[iIndex].type = ccsp_double;
                    else if ( !strcmp(pTemp, "byte") )
                        pParameterValList[iIndex].type = ccsp_byte;

                    CosaPhpExtLog("  Param type %d->%s\n", pParameterValList[iIndex].type, pTemp);
                    /*pop type k/v*/
                    duk_pop(ctx);
                    duk_pop(ctx);

                    if (duk_next(ctx, -1, 1))/*get the value*/
                    {
                        pParameterValList[iIndex].parameterValue = (char*)duk_get_string(ctx, -1);/*FIXME unsafe cast from const char* to char*: fix when replacing ccsp with new system*/
                    
                        if ( pParameterValList[iIndex].type == ccsp_boolean )
                        {
                            /* support true/false or 1/0 for boolean value */
                            if ( !strcmp(pParameterValList[iIndex].parameterValue, "1") )
                            {
                                strcpy(BoolStrBuf, "true");
                                pParameterValList[iIndex].parameterValue = BoolStrBuf;
                            }
                            else if ( !strcmp(pParameterValList[iIndex].parameterValue, "0"))
                            {
                                strcpy(BoolStrBuf, "false");
                                pParameterValList[iIndex].parameterValue = BoolStrBuf;
                            }
                        }
                        CosaPhpExtLog("  Param Value %s\n", pParameterValList[iIndex].parameterValue);
                        /*pop value k/v*/
                        duk_pop(ctx);
                        duk_pop(ctx);
                    }
                    else
                    {
                        CosaPhpExtLog("Subarray invalid\n");
                    }
                }
                else
                {
                    CosaPhpExtLog("Subarray invalid\n");
                }
            }
            else
            {
                CosaPhpExtLog("Subarray invalid\n");
            }
            
            iIndex++;

            /*pop sub array*/
            duk_pop(ctx);
            
        }
        /*pop key and value from main array*/
        duk_pop(ctx);
        duk_pop(ctx);
    }
    /*pop main array*/
    duk_pop(ctx);

    iReturn = 
        CcspBaseIf_setParameterValues
            (
                bus_handle,
                pDestComponentName,
                pDestPath,
                0,
                CCSP_COMPONENT_ID_WebUI,
                pParameterValList,
                iIndex,         /* use the actual count, instead of iParamCount */
                bDbusCommit,
                &pFaultParamName
            );

    if ( CCSP_SUCCESS != iReturn )
    {
        CosaPhpExtLog
            (
                "CcspBaseIf_setParameterValues failed, bCommit:%d, error code = %d, fault parameter = %s.\n",
                bDbusCommit,
                iReturn,
                pFaultParamName ? pFaultParamName : "none"
            );
            
        if (pFaultParamName)
        {
            free(pFaultParamName);
        }

        goto  EXIT1;
    }
    else
    {
        CosaPhpExtLog("CcspBaseIf_setParameterValues succeeded!\n");
        iReturn  = 0;
    }

EXIT1:

    if ( pParameterValList )
    {
        free(pParameterValList);
    }
    
EXIT0:

    RETURN_LONG(iReturn);
}

static duk_ret_t DmExtGetInstanceIds(duk_context *ctx)
{
    char*                           dotstr              = NULL;
    char                            subSystemPrefix[6]  = {0};
    char*                           ppDestComponentName = NULL;
    char*                           ppDestPath          = NULL ;
    int                             iReturn             = 0;
    unsigned int                    InstNum             = 0;
    unsigned int*                   pInstNumList        = NULL;
    unsigned int                    iIndex              = 0;

    if (!parse_parameter(__FUNCTION__, ctx, "s", &dotstr))
    {
        iReturn = CCSP_FAILURE;
        goto  EXIT0;
    }

    //check whether there are subsystem prefix in the dot string
    //Split Subsytem prefix and COSA dotstr if subsystem prefix is found
    CheckAndSetSubsystemPrefix(&dotstr,subSystemPrefix); 

    //Get Destination component 
    iReturn = UiDbusClientGetDestComponent(dotstr, &ppDestComponentName, &ppDestPath,subSystemPrefix);

    if ( iReturn != 0 )
    {
        goto  EXIT0;
    }
    else
    {
        CosaPhpExtLog("DmExtGetInstanceIds -- ObjectTable: %s, destination component: %s, %s\n", dotstr, ppDestComponentName, ppDestPath);
    }

    /*
     *  Get Next Instance Numbes
     */
    iReturn =
        CcspBaseIf_GetNextLevelInstances
            (
                bus_handle,
                ppDestComponentName,
                ppDestPath,
                dotstr,
                &InstNum,
                &pInstNumList
            );

    if (iReturn != CCSP_SUCCESS)
    {
        CosaPhpExtLog("Failed on CcspBaseIf_GetNextLevelInstances, error code = %d.\n", iReturn);
        goto  EXIT1;
    }
    else
    {
        char            StrBuf[24];

        duk_idx_t arr_idx = duk_push_array(ctx);
        duk_push_int(ctx, 0);
        duk_put_prop_index(ctx, arr_idx, 0);

        CosaPhpExtLog("%d of returned values:\n", InstNum);

        for ( iIndex = 0; iIndex < InstNum; iIndex++ )
        {
            snprintf(StrBuf, sizeof(StrBuf) - 1, "%d", pInstNumList[iIndex]);
            duk_push_string(ctx, StrBuf);
            duk_put_prop_index(ctx, arr_idx, iIndex+1);
            CosaPhpExtLog("Instance %d: %s\n", iIndex, StrBuf);
        }

        if (pInstNumList)
        {
            free(pInstNumList);
        }

        iReturn  = 0;
    }

EXIT1:
    
    if ( iReturn == 0 )
    {
        return 1;
    }

EXIT0:

    /*
     * construct return value array
     */
    {
      duk_idx_t arr_idx = duk_push_array(ctx);
      duk_push_int(ctx, iReturn);
      duk_put_prop_index(ctx, arr_idx, 0);
    }
  
    return 1;
}

static duk_ret_t getJWT(duk_context *ctx)
{
    char *pURI = NULL;
    char *pClientId = NULL;
    char *pParams = NULL;
    char *pFileName = NULL;
    int iRet = 0;

    CosaPhpExtLog( "getJWT - Entry\n" );
    if (!parse_parameter(__FUNCTION__, ctx, "ssss", &pURI, &pClientId, &pParams, &pFileName ))
    {
        iRet = 1;
    }
    else
    {
      CosaPhpExtLog( "getJWT - calling SSOgetJWT\n" );
#ifdef COMCAST_SSO
      iRet = SSOgetJWT( pURI, pClientId, pParams, pFileName );
#else
      iRet = 1;
#endif /* COMCAST_SSO */
      CosaPhpExtLog( "getJWT - iRet = %d\n", iRet );
    }
    CosaPhpExtLog("getJWT - exit with value = %d\n", iRet);
    RETURN_LONG(iRet);
}

static const duk_function_list_entry ccsp_cosa_funcs[] = {
  { "getStr", getStr, DUK_VARARGS },
  { "setStr", setStr, DUK_VARARGS },
  { "getInstanceIds", getInstanceIds, DUK_VARARGS },
  { "addTblObj", addTblObj, DUK_VARARGS },
  { "delTblObj", delTblObj, DUK_VARARGS },
  { "DmExtGetStrsWithRootObj", DmExtGetStrsWithRootObj, DUK_VARARGS },
  { "DmExtSetStrsWithRootObj", DmExtSetStrsWithRootObj, DUK_VARARGS },
  { "DmExtGetInstanceIds", DmExtGetInstanceIds, DUK_VARARGS },
  { "getJWT", getJWT, DUK_VARARGS },
  { NULL, NULL, 0 }
};

duk_int_t ccsp_cosa_module_open(duk_context *ctx)
{
  cosa_init();
  duk_push_object(ctx);
  duk_put_function_list(ctx, -1, ccsp_cosa_funcs);
  return 0;
}

