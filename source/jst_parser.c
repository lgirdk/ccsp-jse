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
#include <ctype.h>
#include <memory.h>
#include <unistd.h>
#include "jst.h"
#include "jst_internal.h"

#define JST_OPEN_TAG "<?%"
#define JST_OPEN_LEN 3
#define JST_CLOSE_TAG "?>"
#define JST_CLOSE_LEN 2

#define TMPL_MAX_INC_SZ 256

#if CHAR_BIT != 8
  #pragma message "This code asssumes 8 bit chars"
#endif

#define log_debug_message(...)\
{\
  fprintf(stderr, __VA_ARGS__);\
  CosaPhpExtLog(__VA_ARGS__);\
}

typedef enum template_block_type
{
  template_block_code,
  template_block_string,
  template_block_content
}template_block_type;

typedef struct template_block
{
  char* start;
  size_t len;
  template_block_type type;
  int need_free;
}template_block;

typedef struct growing_buffer
{
  char* data;
  size_t write_len;
  size_t alloc_len;
}growing_buffer;

#define GB_BLOCK_SIZE 100000

void buffer_init(growing_buffer* buf)
{
  memset(buf, 0, sizeof(growing_buffer));
  buf->data = (char*)calloc(GB_BLOCK_SIZE, 1);
  buf->alloc_len = GB_BLOCK_SIZE;
  buf->write_len = 0;
}

void buffer_push(growing_buffer* buf, const char* s, size_t len)
{
  if(buf->alloc_len < buf->write_len + len + 1) /*+ 1 is space for null terminator*/
  {
    if(!buf->data)
      return;

    while(buf->alloc_len < buf->write_len + len + 1)
    {
      buf->alloc_len += GB_BLOCK_SIZE;
    }

    char* rbuf = (char*)realloc(buf->data, buf->alloc_len);
    if(rbuf)
    {
      buf->data = rbuf;
      memset(buf->data + buf->write_len, 0, buf->alloc_len - buf->write_len);
    }
    else
    {
      free(buf->data);
      memset(buf, 0, sizeof(growing_buffer));/*all future calls to buffer_push will return after the if(!buf->data) check above*/
      return;
    }
  }
  strncpy(buf->data + buf->write_len, s, len);
  buf->write_len += len;
}

void buffer_free(growing_buffer* buf)
{
  if(buf->data)
    free(buf->data);
}


#define MAX_PATH_LEN 256
#define MAX_INCLUDE_FILE 16
static int g_is_cgi = 1;
static char g_document_root_path[MAX_PATH_LEN] = {0};
static char g_include_paths[MAX_INCLUDE_FILE][MAX_PATH_LEN] = {{0}};
static int g_include_paths_count = 0;

static void template_write_block(growing_buffer* bufout, template_block* block);
static int template_make_include(char** bufcur, size_t* buflen, template_block* block, char* bufstart);
static int template_make_block(char** bufcur, size_t* buflen, template_block* block);
static void process_includes(char* buf, size_t buflen, growing_buffer* bufout);
static void process_jst(char* buf, size_t buflen, growing_buffer* bufout);
static int template_process(char** buf, size_t* buflen, int top);

static void log_syntax_error(char* err, char* s1, char* cur, char* end)
{
  char ch;

  while(cur != end && *cur != '\n' && *cur != '\r')
    cur++;

  ch = *cur;
  *cur = 0;
  log_debug_message("syntax error. malformed include: %s. line: %s src:%s\n", err, s1, cur);
  *cur = ch;
}

static void template_write_block(growing_buffer* bufout, template_block* block)
{
  size_t i;

  /*
  char tmp = *(block->start + block->len);
  *(block->start + block->len) = 0;
  printf("\n------------------------------\ngot block type=%d len=%d content=\n%s\n------------------------------\n", block->type, block->len, block->start);
  *(block->start + block->len) = tmp;
  */

  /* ignore whitespace only blocks */
  if(block->len == 0)
  {
    //printf("skipping 0 len block\n");
    return;
  }
  for(i = 0; i < block->len; ++i)
    if(!isspace(block->start[i]))
      break;
  if(i == block->len)
  {
    //printf("skipping block which is all whitespace\n");
    return;
  }

  if(block->type == template_block_content)
  {
    buffer_push(bufout, "echo('", 6);
  }
  else if(block->type == template_block_string)
  {
    buffer_push(bufout, "echo(", 5);
  }

  if(block->type == template_block_content)
  {
    for(i = 0; i < block->len; ++i)
    {
      /*line feeds: 
        in order to build a string that is broken by line feeds,
        and in order to preserve the line feed in the string
        so that the client gets back the exact content we have in our file,
        we need to embed an escaped line feed into the string,
        and we need to end the line with an escaped backslash.
          example:
            input : '...foo\n...'
            output: '...foo\\n\\\n ...'
      */
      if(block->start[i] == '\n')
      {
        buffer_push(bufout, "\\n\\", 3);
      }
      /* single quotes must be escaped because we are putting 
         content in a single quoted string */
      else if(block->start[i] == '\'')
      {
        buffer_push(bufout, "\\", 1);
      }
      /* backslash must be escaped because a single backslash 
          inside a string is an escape character prefix.
         This happens if content javascript is escaping something
        and the jst javascript we send to duk needs to print
          the content javascript exactly */
      else if(block->start[i] == '\\')
      {
        buffer_push(bufout, "\\", 1);
      }
      buffer_push(bufout, &block->start[i], 1);
    }
  }
  else
  {
    buffer_push(bufout, block->start, block->len);
  }

  if(block->type == template_block_content)
  {
    buffer_push(bufout, "');", 3);
  }
  else if(block->type == template_block_string)
  {
    buffer_push(bufout, ");", 2);
  }
}

static int template_make_include(char** bufcur, size_t* buflen, template_block* block, char* bufstart)
{
  char* buff;
  char* s1;
  char* cur;
  char* end;
  int valid;
  int quotes;
  int i;
  char include_path[TMPL_MAX_INC_SZ];
  size_t ilen;

  buff = *bufcur;
  end = buff + *buflen;

  s1 = buff;

  block->need_free = 0;

  for(;;)
  {
    valid = 0;
    
    if(s1 >= end || 
      (s1 = strstr(s1, "include")) == NULL)
    {
      block->type = template_block_code;
      block->start = buff;
      block->len = *buflen;
      *bufcur += *buflen;
      *buflen = 0;
      return block->len;
    }

    /*determine if this include is in a js tag 
      by searching backwards and looking for the open tag*/

    if(s1 - bufstart < JST_OPEN_LEN)
    {
      /*too close to start of file to have an open tag
        but its possible this is some kind of html so continue*/
      break;
    }
    else
    {
      cur = s1 - JST_OPEN_LEN;

      for(;;)
      {
        if(!strncmp(cur, JST_OPEN_TAG, JST_OPEN_LEN))
        {
          /*we found the open tag which mean we are inside a tag*/
          valid = 1;
          break;
        }
        else if(!strncmp(cur, JST_CLOSE_TAG, JST_CLOSE_LEN))
        {
          /*we found a close tag which means we aren't in a tag */
          break;
        }
        if(cur == bufstart)
        {
          /*if we hit the start of the file thus we aren't in a tag*/
          break;
        } 
        cur--;
      }
    }

    ilen = strlen("include");

    if(!valid)
    {
      /* this include wasn't inside a js tag, so skip and search for next*/
      s1 += ilen;
      continue; 
    }

    /*we have an include that's inside a js tag
      now check that its not commented out 
      by searching backwards for comment tags*/


    /* check if this include is in a single line comment */
    cur = s1-2;
    for(;;)
    {
      if(cur[0] == '/' && cur[1] == '/')
      {
        /* found the // on the same line so its commented out*/
        valid = 0;
        break;
      }
      else if(cur[0] == '\n' || cur[1] == '\n')
      {
        /* we are on the previous line now so we are good */
        break;
      }
      else if(cur == bufstart)
      {
        /* we have reach the start of the file so we are good */
        break;
      }
      cur--;
    }

    if(!valid)
    {
      /* this include was commented out so search for the next */
      s1 += ilen;
      continue;
    }

    /* check if this include is in a block comment */
    cur = s1-2;
    for(;;)
    {
      if(cur[0] == '/' && cur[1] == '*')
      {
        /*we found the start of a comment thus we are in a comment*/
        valid = 0;
        break;
      }
      else if(cur[0] == '*' && cur[1] == '/')
      {
        /*we found the end of a comment thus we are not in a comment*/
        break;
      }
      else if(cur == bufstart)
      {
        break;
      }
      cur--;
    }

    if(!valid)
    {
      /* this include was commented out so search for the next */
      s1 += ilen;
      continue;
    }

    /* The only valid chars in front should be whitespace or semicolon */  
    /* FIXME - i'm not sure this captures every possible syntax correctly */
    cur = s1;
    cur--;
    if(*cur != ' ' && *cur != '\t' && *cur != ';' && *cur != '\n')
    {
      /* this could be a variable or another function */
      s1 += ilen;
      continue;
    }

    /*search ahead for ( */
    cur = s1+7;
    for(;;)
    {
      if(cur >= end)
      {
        /*reached end of file*/
        valid = 0;
        break;
      }
      if(*cur == '(')
      {
        break;
      }
      if(*cur != ' ')/*only a space is allowed*/
      {
        /*this could be a function which starts with include, like includeFoo()*/
        valid = 0;
        break;
      }
      cur++;
    }

    if(!valid)
    {
      s1 += ilen;
      continue;
    }
    cur++;

    /* at this point we know its ' include(' */

    /*if there is stuff before the include that hasn't been written
      send it back to get written */
    if(s1 != buff)
    {
      block->type = template_block_code;
      block->start = buff;
      block->len = s1 - buff;
      *bufcur = s1;
      *buflen -= block->len;
      return block->len;
    }

    /* now look for syntax errors that should kill the load 
       for anything not valid, log a syntax error and return 0 */

    /* search ahead for single or double quote */
    for(;;)
    {
      if(cur >= end)
      {
        /*reached end of file*/
        valid = 0;
        break;
      }
      if(*cur == '\'')
      {
        quotes = 1;
        break;
      }
      else if(*cur == '\"')
      {
        quotes = 2;
        break;
      }

      /*if anything other then a space exist before the quote, then it could be a variable,
        in which case we let the 'include' function (in jst_functions.c) to process it at runtime*/
      if(*cur != ' ')
      {
        log_debug_message("runtime include statement found\n");
        valid = 0;
        break;
      }

      cur++;
    }

    if(!valid)
    {
      s1 += ilen;
      continue;
    }

    cur++;

    /* now copy the include path characters while searching for the end quote */
    i = 0; 
    for(;;)
    {
      if(cur >= end)
      {
        log_syntax_error("include path at eof", s1, cur, end);
        valid = 0;
        break;
      }

      if((quotes == 1 && *cur == '\'') || 
         (quotes == 2 && *cur == '\"'))
      {
        break;
      }

      if(i == TMPL_MAX_INC_SZ)
      {
        log_syntax_error("include path too long", s1, cur, end);
        valid = 0;
        break;
      }

      if((quotes == 1 && *cur == '\"') || 
         (quotes == 2 && *cur == '\''))
      {
        log_syntax_error("include quotes missmatch", s1, cur, end);
        valid = 0;
        break;
      }

      include_path[i++] = *(cur++);
    }

    if(!valid)
    {
      s1 += ilen;
      continue;
    }

    cur++;

    /* nul terminate the path */
    include_path[i] = 0;

    /* check for closing ) */
    for(;;)
    {
      if(cur >= end)
      {
        log_syntax_error("include path at eof", s1, cur, end);
        valid = 0;
        break;
      }

      if(*cur == ')')
      {
        break;
      }

      if(*cur != ' ')
      {
        log_syntax_error("invalid character in include", s1, cur, end);
        valid = 0;
        break;
      }
      cur++;
    }

    if(!valid)
    {
      s1 += ilen;
      continue;
    }

    cur++;


    /* check for the ending ; or ?>*/
    for(;;)
    {
      if(cur >= end)
      {
        log_syntax_error("include path at eof", s1, cur, end);
        valid = 0;
        break;
      }

      if(*cur == ';')
      {
        break;
      }

      if(strncmp(cur, "?>", 2) == 0)
      {
        cur--;/*back out so that template tag stays*/
        break;
      }

      if(*cur != ' ')
      {
        log_syntax_error("invalid character in include", s1, cur, end);
        valid = 0;
        break;
      }
      cur++;
    }

    if(!valid)
    {
      s1 += ilen;
      continue;
    }

    cur++;

    /* the include was parsed successfully
       now load the file path recursively */
    if(load_template_file(include_path, &block->start, &block->len, 0))
    {
//TODO - does the data alloced for block->start ever get freed ????
      //printf("=============================\ninclude file contents:\n%s\n=============================\n", block->start);
      block->type = template_block_code;
      block->need_free = 1;
      *bufcur = cur;
      *buflen -= cur - buff;
      return block->len;
    }
    else
    {
      /*if include had previously been included once*/
      block->type = template_block_code;
      *bufcur = cur;
      *buflen -= cur - buff;
      return 1;
    }
  }

  return 0;
}

static int template_make_block(char** bufcur, size_t* buflen, template_block* block)
{
  char* buff;
  char* s1;
  char* s2;
  template_block_type type;

  buff = *bufcur;

  s1 = strstr(buff, JST_OPEN_TAG);

  if(s1)
  {
    if(s1 != buff)
    {
      block->type = template_block_content;
      block->start = buff;
      block->len = s1 - buff;
      *bufcur = s1;
      *buflen -= block->len;
      return block->len;
    }
    else
    {
      if(s1[JST_OPEN_LEN] == '=')
      {
        s1 += JST_OPEN_LEN+1;
        type = template_block_string;
      }
      else
      {
        s1 += JST_OPEN_LEN;
        type = template_block_code;
      }

      s2 = strstr(s1, JST_CLOSE_TAG);

      if(s2)
      {
        block->type = type;
        block->start = s1;
        block->len = s2-s1;
        *bufcur = s2+JST_CLOSE_LEN;
        *buflen -= *bufcur - buff;
        return block->len;
      }
      else
      {
        //error no closing ?> found
        return 0;
      }
    }
  }
  else
  {
    block->type = template_block_content;
    block->start = buff;
    block->len = *buflen;
    *bufcur += *buflen;
    *buflen = 0;
    return block->len;
  }
  return 0;
}

static void process_jst(char* buf, size_t buflen, growing_buffer* tbuf)
{
  char* bufcur;
  size_t bufcurlen;
  template_block block;

  bufcur = buf;
  bufcurlen = buflen;

  for(;;)
  {
    if(template_make_block(&bufcur, &bufcurlen, &block))
    {
      template_write_block(tbuf, &block);
    }
    else
    {
      break;
    }
  }
}

static void process_includes(char* buf, size_t buflen, growing_buffer* tbuf)
{
  char* bufcur;
  size_t bufcurlen;
  template_block block;

  bufcur = buf;
  bufcurlen = buflen;

  for(;;)
  {
    if(template_make_include(&bufcur, &bufcurlen, &block, buf))
    {
      template_write_block(tbuf, &block);
      if(block.need_free)
        free(block.start);
    }
    else
    {
      break;
    }
  }
}

static int template_process(char** buf, size_t* buflen, int top)
{
  /*make 2 passes 
    1) process include statements into intermediary jst
    2) process intermediary jst into final js code
  */
  char* prefix;
  size_t prefix_len;
  char* suffix;
  size_t suffix_len;
  char filepath[MAX_PATH_LEN];

  growing_buffer tbuf1;
  growing_buffer tbuf2;

  buffer_init(&tbuf1);
  buffer_init(&tbuf2);

#ifdef NO_PROCESS_INCLUDES
  buffer_push(&tbuf1, *buf, *buflen);
#else
  process_includes(*buf, *buflen, &tbuf1);
#endif

#if 0
  printf("----------- after processing includes -----------\n");
  printf("%s\n", tbuf1.data);
  printf("-------------------------------------------------\n");
#endif

  if(top)
  {
    snprintf(filepath, MAX_PATH_LEN, "%sjst_prefix.js", g_document_root_path);

    if(!read_file(filepath, &prefix, &prefix_len))
    {
      log_debug_message("failed to open %s\n", filepath);
      buffer_free(&tbuf1);
      free(*buf);
      *buf = 0;
      *buflen = 0;
      return 0;
    }

    snprintf(filepath, MAX_PATH_LEN, "%sjst_suffix.js", g_document_root_path);

    if(!read_file(filepath, &suffix, &suffix_len))
    {
      log_debug_message("failed to open %s\n", filepath);
      buffer_free(&tbuf1);
      free(prefix);
      free(*buf);
      *buf = 0;
      *buflen = 0;
      return 0;
    }

    buffer_push(&tbuf2, prefix, prefix_len);
  }

  process_jst(tbuf1.data, tbuf1.write_len, &tbuf2);

  if(top)
  {
    buffer_push(&tbuf2, suffix, suffix_len);
    //buffer_push(&tbuf2, "\0", 1); /*not needed as growing_buffer memsets its buffer to 0*/

    free(prefix);
    free(suffix);
  }

  free(*buf);
  *buf = tbuf2.data;
  *buflen = tbuf2.write_len;

  buffer_free(&tbuf1);

  /*we pass tbuf2 data back so don't call buffer_free on it*/
  return *buflen;
}

int load_template_file(const char *filename, char** bufout, size_t* lenout, int top)
{
  char* buf;
  size_t buflen;
  size_t rc;
  int i;
  char filepath[MAX_PATH_LEN];
  const char* pscriptname = filename;

  log_debug_message("load_template_file filename=%s top=%d\n", filename, top);

  *bufout = NULL;
  *lenout = 0;

  if(top)
  {
    char* pgi;

    /*cleanup any previous passes through here*/
    g_document_root_path[0] = 0;
    for(i = 0; i < g_include_paths_count; ++i)
      g_include_paths[i][0] = 0;
    g_include_paths_count = 0;
    
    /*are we running as cgi or stand-alone*/
    pgi = getenv("GATEWAY_INTERFACE");
    if(pgi && strncmp(pgi, "CGI/", 4) == 0)
      g_is_cgi = 1;
    else
      g_is_cgi = 0;

    /*determine document root
    this is where the jst_prefix.js/jst_suffix.js files should live 
    and any include path is treated as relative to this */

    if(g_is_cgi)
    {
      /*for cgi we can use cgi env vars to figure it out*/
      char* pscriptfile = getenv("SCRIPT_FILENAME");  /* eg: /tmp/www/actionHandler/ajaxSet_index_userbar.jst */
      pscriptname = getenv("SCRIPT_NAME");            /* eg: /actionHandler/ajaxSet_index_userbar.jst */

      if(pscriptname && pscriptfile)
      {
        char* p1;

        if(pscriptname[0] == '/')
          pscriptname++;

        if(strlen(pscriptfile) > MAX_PATH_LEN)
        {
          log_debug_message("SCRIPT_FILENAME exceeds our max supported path len\n");
          return 0;
        }

        p1 = strstr(pscriptfile, pscriptname);
        if(p1)
        {
          size_t rootlen = p1 - pscriptfile;
          if(rootlen < MAX_PATH_LEN)
          {
            strncpy(g_document_root_path, pscriptfile, rootlen);
          }
        }
        else
        {
          log_debug_message("SCRIPT_NAME not found in SCRIPT_FILENAME\n");
          return 0;
        }
      }
      else
      {
        log_debug_message("SCRIPT_NAME/FILENAME env var missing\n");
        return 0;
      }

      log_debug_message("cgi root directory is %s\n", g_document_root_path);
    }
    else
    {
      /*for stand alone use the current work directory*/
      if(!getcwd(g_document_root_path, MAX_PATH_LEN-1))
      {
        log_debug_message("failed to get current working directory\n");
        return 0;
      }
      strcat(g_document_root_path, "/");

      log_debug_message("non-cgi root directory is %s\n", g_document_root_path);
    }
  }

  log_debug_message("root=%s  path=%s\n", g_document_root_path, pscriptname);
  i = snprintf(filepath, MAX_PATH_LEN, "%s%s", g_document_root_path, pscriptname);

  if(i < 0 || i >= MAX_PATH_LEN)
  {
    log_debug_message("file path too long %s\n", filename);
    return 0;
  }

  /*determine if file has already been included and include only once*/
  if(!top)
  {
    log_debug_message("checking if %s has already been included\n", filepath);
    for(i = 0; i < g_include_paths_count; ++i)
    {
      if(strcmp(g_include_paths[i], filepath) == 0)
      {
        log_debug_message("skipping %s, already included once\n", filename);
        return 0;
      }
    }
  }

  if(g_include_paths_count == MAX_INCLUDE_FILE-2)
  {
    log_debug_message("max number includes %d has been reached\n", MAX_INCLUDE_FILE);
    return 0;
  }

  strcpy(g_include_paths[g_include_paths_count++], filepath);

  log_debug_message("load_template_file:%s filepath=%s root:%s top:%d\n", filename, filepath, g_document_root_path, top);

  if(!read_file(filepath, &buf, &buflen))
  {
    return 0;
  }

  rc = strlen(filename);
  if(rc > 4 && !strcmp(filename + rc - 4, ".jst"))
    if(!template_process(&buf, &buflen, top))
      return 0;

  *bufout = buf;
  *lenout = buflen;
  return buflen;
}

