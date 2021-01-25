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
#include "jst_internal.h"
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define COSA_PHP_EXT_LOG_FILE_NAME  "/var/log/cosa_php_ext.log"
#define COSA_PHP_EXT_DEBUG_FILE "/tmp/cosa_php_debug"
static int debugFlag = 0;

void CosaPhpExtLog(const char* format, ...)
{
  if (debugFlag)
  {
    FILE*              pFile       = NULL;
    mode_t             origMod     = umask(0);
    struct timeval     tv;
    struct tm          tm;

    pFile = fopen(COSA_PHP_EXT_LOG_FILE_NAME, "a");

    if ( pFile )
    {
      va_list vl;

      /* print the current timestamp */
      gettimeofday(&tv, NULL);
      tm = *localtime(&tv.tv_sec);
      fprintf(pFile,
        "%04d-%02d-%02d %02d-%02d-%02d:%06ld ",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec,
        tv.tv_usec
      );

      va_start (vl, format);
      vfprintf(pFile, format, vl);
      va_end (vl);

      fclose(pFile);
    }

    umask(origMod);
  }
}

void init_logger()
{
  FILE *fp = NULL;
  /* If file exists, we'll open the debug flag */
  fp = fopen(COSA_PHP_EXT_DEBUG_FILE, "r");
  if (fp)
  {
    debugFlag = 1;
    fclose(fp);
  }
}

int parse_parameter(const char* func, duk_context *ctx, const char* types, ...)
{
  static const int overrun_guard = 10;
  int i;
  int success;
  va_list vl;

  success = 1;
  va_start(vl, types);

  for(i = 0; types[i] && i < overrun_guard && success; ++i)
  {
    switch(types[i])
    {
      case 's':
      {
        const char** pstr = va_arg(vl, const char**);
        *pstr = duk_get_string(ctx, i);
        if(*pstr)
        {
          if(!strlen(*pstr))
          {
            CosaPhpExtLog("%s - parameter %d (string) empty\n", func, i);
            success = 0;
          }
        }
        else
        {
          CosaPhpExtLog("%s - parameter %d (string) missing\n", func, i);
          success = 0;
        }
      }
      break;
      case 'b':
      {
        duk_bool_t* pbool = va_arg(vl, int*);
        if(duk_is_boolean(ctx,i))
        {
          *pbool = duk_get_boolean(ctx,i);
        }
        else
        {
          CosaPhpExtLog("%s - parameter %d (bool) missing\n", func, i);
          success = 0;
        }
      }
      break;
      case 'o':
      {
        duk_idx_t* pidx = va_arg(vl, int*);
        if(duk_is_object(ctx,i))
        {
          *pidx = i;
        }
        else
        {
          CosaPhpExtLog("%s - parameter %d (object) missing\n", func, i);
          success = 0;
        }
      }
      break;
      case 'n':
      {
        duk_double_t* pdouble = va_arg(vl, double*);
        if(duk_is_number(ctx,i))
        {
          *pdouble = duk_get_number(ctx, i);
        }
        else
        {
          CosaPhpExtLog("%s - parameter %d (number) missing\n", func, i);
          success = 0;
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

  return success;
}

int read_file(const char *filename, char** bufout, size_t* lenout)
{
  FILE* pf;
  size_t size;
  size_t rc;
  char* buf;

  CosaPhpExtLog( "read_file %s\n", filename );

  errno = 0;
  pf = fopen(filename, "r");
  if(!pf)
  {
    char * serr = strerror(errno);
    CosaPhpExtLog( "read_file cannot open file:%s error:%s\n", filename, serr );
    fprintf(stderr, "Error: cannot open file:%s error:%s\n", filename, serr);
    return 0;
  }

  fseek(pf, 0, SEEK_END); 
  size = ftell(pf);
  rewind(pf);

  buf = (char*)calloc(size+1, 1);
  if(!buf)
  {
    fclose(pf);
    fprintf(stderr, "Error: malloc oom %s\n", filename);
    return 0;
  }

  rc = fread(buf,1,size,pf);
  if(rc != size)
  {
    free(buf);
    fclose(pf);
    fprintf(stderr, "Error: read failed %s\n", filename);
    return 0;
  }

  buf[rc] = '\0';

  fclose(pf);

  *bufout = buf;
  *lenout = size;
  return *lenout;
}

