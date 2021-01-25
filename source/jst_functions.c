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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "jst_internal.h"
#include "jst.h"

/* OpenSSL includes */
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <libintl.h>

static duk_ret_t do_getenv(duk_context *ctx)
{
  char* sval = NULL;
  char* ret = NULL;

  if (!parse_parameter(__FUNCTION__, ctx, "s", &sval))
    RETURN_STRING("failed to parse parameters");

  ret = getenv(sval);

  if(ret)
  {
    RETURN_STRING(ret);
  }
  else
  {
    RETURN_FALSE;
  }
}

static duk_ret_t do_bindtextdomain(duk_context *ctx)
{
  char* domainname = NULL;
  char* locale = NULL;
  char* ret = NULL;

  if (!parse_parameter(__FUNCTION__, ctx, "ss", &domainname, &locale))
    RETURN_STRING("failed to parse parameters");

  ret = bindtextdomain(domainname, locale);

  if(ret)
  {
    RETURN_STRING(ret);
  }
  else
  {
    RETURN_FALSE;
  }
}

static duk_ret_t do_bind_textdomain_codeset(duk_context *ctx)
{
  char* domainname = NULL;
  char* codeset = NULL;
  char* ret = NULL;

  if (!parse_parameter(__FUNCTION__, ctx, "ss", &domainname, &codeset))
    RETURN_STRING("failed to parse parameters");

  ret = bind_textdomain_codeset(domainname, codeset);

  if(ret)
  {
    RETURN_STRING(ret);
  }
  else
  {
    RETURN_FALSE;
  }
}

static duk_ret_t do_textdomain(duk_context *ctx)
{
  char* domainname = NULL;
  char* ret = NULL;

  if (!parse_parameter(__FUNCTION__, ctx, "s", &domainname))
    RETURN_STRING("failed to parse parameters");

  ret = textdomain(domainname);

  if(ret)
  {
    RETURN_STRING(ret);
  }
  else
  {
    RETURN_FALSE;
  }
}

static duk_ret_t do_gettext(duk_context *ctx)
{
  char* msgid = NULL;
  char* ret = NULL;

  if (!parse_parameter(__FUNCTION__, ctx, "s", &msgid))
    RETURN_STRING("failed to parse parameters");

  ret = gettext(msgid);

  if(ret)
  {
    RETURN_STRING(ret);
  }
  else
  {
    RETURN_FALSE;
  }
}

static duk_ret_t do_exec(duk_context *ctx)
{
  char* command;
  char *line = NULL;
  size_t len = 0;
  ssize_t nread;
  duk_idx_t idx;
  int index = 0;

  idx = duk_push_array(ctx);

  if (!parse_parameter(__FUNCTION__, ctx, "s", &command))
    return 1;

  CosaPhpExtLog("exec command=%s\n", command);

  FILE* pipe = popen(command, "r");
  if (!pipe)
  {
    CosaPhpExtLog("exec failed to open pipe\n");
    duk_pop(ctx);
    return 1;    
  }

  while((nread = getline(&line, &len, pipe)) != -1)
  {
    CosaPhpExtLog("exec line: %s\n", line);
    duk_push_string(ctx, line);
    duk_put_prop_index(ctx, idx, index++);
  }

  free(line);
  pclose(pipe);

  return 1;
}

static duk_ret_t do_sleep(duk_context *ctx)
{
  double dval;

  if (!parse_parameter(__FUNCTION__, ctx, "n", &dval))
    RETURN_FALSE;

  /*convert from seconds to microseconds*/
  unsigned int sleep_time = (unsigned int)(dval * 1000000.0);

  CosaPhpExtLog("sleeping for %u ms\n", sleep_time );

  usleep(sleep_time);

  RETURN_TRUE;
}

static duk_ret_t do_fopen(duk_context *ctx)
{
  char* path;
  char* mode;

  //CosaPhpExtLog("fopen\n");

  if (!parse_parameter(__FUNCTION__, ctx, "ss", &path, &mode))
    RETURN_FALSE;

  //CosaPhpExtLog("fopen path:%s mode:%s\n", path, mode);

  errno = 0;
  FILE* handle = fopen(path, mode);

  if(!handle)
  {
    CosaPhpExtLog("fopen failed to open path:%s mode:%s error:%s\n", path, mode, strerror(errno));
    RETURN_FALSE;
  }

  //CosaPhpExtLog("fopen handle=%p\n", handle);

  RETURN_LONG((double)(long int)handle);
}
static duk_ret_t do_fclose(duk_context *ctx)
{
  double number;
  FILE* handle;

  //CosaPhpExtLog("fclose\n");

  if (!parse_parameter(__FUNCTION__, ctx, "n", &number))
    RETURN_FALSE;

  handle = (FILE*)(long int)number;

  //CosaPhpExtLog("fclose handle=%p\n", handle);

  if(fclose(handle) == 0)
  {
    RETURN_TRUE;
  }
  else
  {
    RETURN_FALSE;
  }
}
static duk_ret_t do_fread(duk_context *ctx)
{
  double number;
  double dlength;
  FILE* handle;
  void* buffer;
  size_t len, rlen, filesize;

  if (!parse_parameter(__FUNCTION__, ctx, "nn", &number, &dlength))
    RETURN_FALSE;

  handle = (FILE*)(long int)number;
  len = (size_t)dlength;

  fseek(handle, 0, SEEK_END);
  filesize = ftell(handle);
  fseek(handle, 0, SEEK_SET);

  if(len > filesize)
    len = filesize;

  buffer = duk_push_fixed_buffer(ctx, len+1);/*will this get garbage collected ?*/

  rlen = fread(buffer, 1, len, handle); 

  duk_compact(ctx, -1);

  duk_buffer_to_string(ctx, -1);

  if(rlen == len || feof(handle))
  {
    return 1;
  }
  else
  {
    CosaPhpExtLog("fread failed\n");
    RETURN_FALSE;
  }
}
static duk_ret_t do_fwrite(duk_context *ctx)
{
  double number;
  char* text;
  FILE* handle;
  size_t len, rlen;

  //CosaPhpExtLog("fwrite\n");

  if (!parse_parameter(__FUNCTION__, ctx, "ns", &number, &text))
    RETURN_FALSE;

  handle = (FILE*)(long int)number;

  //CosaPhpExtLog("fwrite handle=%p text=%s\n", handle, text);

  len = strlen(text);

  rlen = fwrite(text, 1, len, handle); 

  if(rlen == len)
  {
    //CosaPhpExtLog("fwrite succes\n");
    RETURN_LONG(len);
  }
  else
  {
    //CosaPhpExtLog("fwrite error only wrote %ul of %ul bytes\n", rlen, len);
    RETURN_LONG(rlen);
  }
}
static duk_ret_t do_fgets(duk_context *ctx)
{
  double number;
  FILE* handle;
  char line[1000];/*TODO improve how we handle the line buffer*/

  if (!parse_parameter(__FUNCTION__, ctx, "n", &number))
    RETURN_FALSE;

  handle = (FILE*)(long int)number;

  //CosaPhpExtLog("fgets handle=%p\n", handle);

  if(!fgets(line, 1000, handle))
  {
    if(!feof(handle))
    {
      CosaPhpExtLog("fgets read error\n");
      RETURN_FALSE;
    }
  }

  RETURN_STRING(line);
}

static duk_ret_t do_feof(duk_context *ctx)
{
  double number;
  FILE* handle;

  if (!parse_parameter(__FUNCTION__, ctx, "n", &number))
    RETURN_FALSE;

  handle = (FILE*)(long int)number;

  //CosaPhpExtLog("fgets feof=%d\n", feof(handle));

  if(feof(handle))
  {
    RETURN_TRUE;
  }
  else
  {
    RETURN_FALSE;
  }
}

static duk_ret_t do_fseek(duk_context *ctx)
{
  double number;
  double dposition;
  long int position;
  FILE* handle;

  if (!parse_parameter(__FUNCTION__, ctx, "nn", &number, &dposition))
    RETURN_FALSE;

  handle = (FILE*)(long int)number;
  position = (long int)dposition;

  //CosaPhpExtLog("fseek %ld\n", position);

  if(fseek(handle, (long int)position, SEEK_SET) == 0)
  {
    RETURN_TRUE;
  }
  else
  {
    CosaPhpExtLog("fseek %ld failed\n", position);
    RETURN_FALSE;
  }
}

static duk_ret_t do_is_readable(duk_context *ctx)
{
  char* path;
  FILE* handle;

  if (!parse_parameter(__FUNCTION__, ctx, "s", &path))
    RETURN_FALSE;

  handle = fopen(path, "r");
  if(handle)
  {
    fclose(handle);
    RETURN_TRUE;
  }
  else
  {
    RETURN_FALSE;
  }
}

static duk_ret_t do_filesize(duk_context *ctx)
{
  char* path;
  FILE* handle;
  long size;

  if (!parse_parameter(__FUNCTION__, ctx, "s", &path))
    RETURN_FALSE;

  handle = fopen(path, "r");
  if(handle)
  {
    fseek(handle, 0, SEEK_END);
    size = ftell(handle);
    fclose(handle);
    RETURN_LONG(size);
  }
  else
  {
    RETURN_FALSE;
  }
}

static duk_ret_t do_logger(duk_context *ctx)
{
  char* log;

  if (!parse_parameter(__FUNCTION__, ctx, "s", &log))
  {
    CosaPhpExtLog("Failed to do logging\n");
    RETURN_FALSE;
  }

  CosaPhpExtLog("JSTLOGS:%s\n", log);

  return 1;

}

static duk_ret_t do_include(duk_context *ctx)
{
  char* filename;
  char* buffer;
  size_t length;

  if (!parse_parameter(__FUNCTION__, ctx, "s", &filename))
  {
    CosaPhpExtLog("Failed to do include\n");
    RETURN_FALSE;
  }

  if(load_template_file(filename, &buffer, &length, 0))
  {
#ifdef DUMP_INCLUDE_FILES
    FILE* dmpFile = 0;
    char path[256];
    static int fileCount = 0;
    sprintf(path, "/tmp/include%d.dmp", fileCount++);
    dmpFile = fopen(path, "w");
    if(dmpFile)
      fwrite(buffer, 1, length-1, dmpFile); 
#endif
    duk_push_string(ctx, buffer);
    duk_push_string(ctx, filename);
    if (duk_pcompile(ctx, 0) != 0) {
      CosaPhpExtLog("do_include compile failed: %s\n", duk_safe_to_string(ctx, -1));
    } else {
      duk_call(ctx, 0);
    }
    duk_pop(ctx);
    free(buffer);
#ifdef DUMP_INCLUDE_FILES
    if(dmpFile)
      fclose(dmpFile);
#endif
    RETURN_TRUE;
  }
  else
  {
    /*if its a jst that has already been loaded we'll come here so its not an error*/
    RETURN_FALSE;
  }
}

static duk_ret_t do_openssl_verify_with_cert(duk_context *ctx)
{
  char* filepath;
  char* token;
  char* sig2verify;
  char* alg;

  BIO* bio = NULL;
  X509* cert = NULL;
  EVP_PKEY * key = NULL;
  const EVP_MD *mdtype = NULL;
  EVP_MD_CTX *md_ctx = NULL;
  int err = 0;
  int ok = 0;

  if (!parse_parameter(__FUNCTION__, ctx, "ssss", &filepath, &token, &sig2verify, &alg))
  {
    CosaPhpExtLog("openssl_verify_with_cert: failed to parse parameters\n");
    RETURN_FALSE;
  }

  //open certificate file
  if(memcmp(filepath, "file://", sizeof("file://")-1) != 0)
  {
    CosaPhpExtLog("openssl_verify_with_cert: file %s doesn't begin with 'file://'\n", filepath);
    RETURN_FALSE;
  }
  filepath += sizeof("file://") - 1;
  bio = BIO_new_file(filepath, "rb") ;
  if(!bio)
  {
    CosaPhpExtLog("openssl_verify_with_cert: failed open file %s\n", filepath);
    RETURN_FALSE;
  }

  //read public key
  cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
  if(cert)
  {
    key = X509_get_pubkey(cert);
    if (!key) 
    {
      CosaPhpExtLog("openssl_verify_with_cert: X509_get_pubkey failed with error: %d (%s)\n", ERR_peek_last_error(), ERR_reason_error_string(ERR_peek_last_error()));
    }
    X509_free(cert);
  }
  else
  {
    key = PEM_read_bio_PUBKEY(bio, NULL, 0, NULL);
  }

  BIO_free(bio);

  if(!key)
  {
    CosaPhpExtLog("openssl_verify_with_cert: failed read public key from %s\n", filepath);
    RETURN_FALSE;
  }

  mdtype = EVP_get_digestbyname(alg);
  if(!mdtype)
  {
    CosaPhpExtLog("openssl_verify_with_cert: EVP_get_digestbyname failed for %s\n", alg);
    EVP_PKEY_free(key);
    RETURN_FALSE;
  }

  md_ctx = EVP_MD_CTX_create();
  if(!md_ctx)
  {
    CosaPhpExtLog("openssl_verify_with_cert: EVP_MD_CTX_create failed\n");
    EVP_PKEY_free(key);
    RETURN_FALSE;
  }

  if(EVP_VerifyInit (md_ctx, mdtype))
  {
    if(EVP_VerifyUpdate (md_ctx, token, strlen(token)))
    {
      err = EVP_VerifyFinal(md_ctx, (unsigned char *)sig2verify, (unsigned int)strlen(sig2verify), key);
      if(err < 0)
      {
        CosaPhpExtLog("openssl_verify_with_cert: EVP_VerifyFinal failed error:%d\n", err);
      }
      else
      {
        ok = 1;
        CosaPhpExtLog("openssl_verify_with_cert: EVP_VerifyFinal success\n");
      }
    }
    else
    {
      CosaPhpExtLog("openssl_verify_with_cert: EVP_VerifyUpdate failed\n");
    }
  }
  else
  {
    CosaPhpExtLog("openssl_verify_with_cert: EVP_VerifyInit failed\n");
  }
  EVP_MD_CTX_destroy(md_ctx);
  EVP_PKEY_free(key);
  if(ok)
  {
    RETURN_TRUE;
  }
  else
  {
    RETURN_FALSE;
  }
}

static const duk_function_list_entry ccsp_functions_funcs[] = {
  { "getenv", do_getenv, 1 },
  { "bindtextdomain", do_bindtextdomain, 2 },
  { "bind_textdomain_codeset", do_bind_textdomain_codeset, 2 },
  { "textdomain", do_textdomain, 1 },
  { "gettext", do_gettext, 1 },
  { "exec", do_exec, 1 },
  { "sleep", do_sleep, 1 },
  { "fopen", do_fopen, 2 },
  { "fclose", do_fclose, 1 },
  { "fwrite", do_fwrite, 2 },
  { "fread", do_fread, 2 },
  { "fgets", do_fgets, 1 },
  { "feof", do_feof, 1 },
  { "fseek", do_fseek, 2 },
  { "is_readable", do_is_readable, 1 },
  { "filesize", do_filesize, 1 },
  { "logger", do_logger, 1 },
  { "include", do_include, 1 },
  { "openssl_verify_with_cert", do_openssl_verify_with_cert, 4 },    
  { NULL, NULL, 0 }
};

duk_ret_t ccsp_functions_module_open(duk_context *ctx)
{
  duk_push_object(ctx);
  duk_put_function_list(ctx, -1, ccsp_functions_funcs);
  return 1;
}
