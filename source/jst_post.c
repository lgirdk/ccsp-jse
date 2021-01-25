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
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include "jst_internal.h"

char* post_data = NULL;

static duk_ret_t get_post(duk_context *ctx)
{
  if(post_data)
  {
    duk_push_string(ctx, post_data); 
    free(post_data); //freeing here is ok only because jst_prefix.js calls getPost once
    post_data = NULL;
    return 1;
  }
  else
  {
    RETURN_FALSE;
  }
}

static const duk_function_list_entry ccsp_post_funcs[] = {
  { "getPost", get_post, 0 },
  { NULL, NULL, 0 }
};

duk_ret_t ccsp_post_module_open(duk_context *ctx)
{
  const char* env_content_len;
  int content_len;
  char* str;

  duk_push_object(ctx);
  duk_put_function_list(ctx, -1, ccsp_post_funcs);

  env_content_len = getenv("CONTENT_LENGTH");
  if(env_content_len)
  {
    content_len = atoi(env_content_len);
    if(content_len > 0)
    {
      post_data = (char*)malloc(content_len + 1);
      str = fgets(post_data, content_len + 1, stdin);
      (void)str;

#if 0 //enable if you need to debug and see what the post data looks like
      {
        FILE* pfile = fopen("/tmp/postFile", "w");
        fwrite(post_data, content_len, 1, pfile);
        fclose(pfile);
      }      
#endif
    }
  }

  return 1;
}


