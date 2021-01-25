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
#include <dirent.h>
#include <vector>
#include <string>
#include <getopt.h>
#include <unistd.h>
#include <ctype.h>
#include <memory.h>
#include <stdarg.h>

using namespace std;

bool getFilesInDirectory(const string& path, vector<string>& files, const string& match, bool recurse = false)
{
  DIR *dir;
  struct dirent *ent;

  //printf("enter %s\n", path.c_str());
  if ((dir = opendir (path.c_str())) != NULL)
  {
    while ((ent = readdir (dir)) != NULL)
    {
      if(ent->d_type == DT_DIR && ent->d_name[0] != '.' && recurse)
      {
        //printf("dir %s\n", ent->d_name);
        string subdir = path + ent->d_name + "/";
        getFilesInDirectory(subdir, files, match, recurse);
      }
      else if(ent->d_type == DT_REG)
      {
        string file = ent->d_name;
        if(!match.empty())
        {
          if(file.length() >= match.length())
          {
             if(file.substr(file.length() - match.length()) == match)
              files.push_back(path + ent->d_name);
          }
        }
        else
          files.push_back(path + ent->d_name);
      }
    };
    closedir (dir);
    return true;
  }
  else
  {
    perror ("error");
    return false;
  }
}

int readFile(const char *filename, char** bufout, size_t* lenout)
{
  FILE* pf;
  size_t size;
  size_t rc;
  char* buf;

  pf = fopen(filename, "r+");
  if(!pf)
  {
    fprintf(stderr, "Error: cannot open file:%s\n", filename);
    return 0;
  }

  fseek(pf, 0, SEEK_END); 
  size = ftell(pf);
  rewind(pf);

  buf = new char[size];
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

  fclose(pf);

  *bufout = buf;
  *lenout = size;
  return *lenout;
}

int writeFile(const char *filename, char* buf, size_t len)
{
  FILE* pf;
  size_t rc;

  errno = 0;
  pf = fopen(filename, "w");
  if(!pf)
  {
    fprintf(stderr, "Error: cannot open file:%s\n", filename);
    return 0;
  }

  rc = fwrite(buf,1,len,pf);
  if(rc != len)
  {
    fprintf(stderr, "Error: write failed %s\n", filename);
    return 0;
  }  

  fclose(pf);
  return rc;
}

class PhpProcessor
{
public:
  PhpProcessor(bool all = false) : all_(all), inquote_(false) {}
  bool all(){ return all_; }
  virtual void onTagOpen(){};
  virtual void onTagClose(const char** pps, char** ppr){};
  virtual void onChar(const char** pps, char** ppr) = 0;
  void setInQuote(bool in)
  {
    inquote_ = in;
  }
  bool inQuote()
  {
    return inquote_;
  }
  void addLabel(char** pr, const char* plabel)
  {
    char buffer[100];
    sprintf(buffer, "/*p2j-%s*/", plabel);
    strcpy(*pr, buffer);
    *pr += strlen(buffer);
  }
  static void printError(const char* ps, const char* format, ...)
  {
    int line = 1;
    const char* p = content;
    const char* lastline = content;
    while(p != ps)
    {
      if(*p == '\n')
      {
        lastline = p;
        line++;
      }
      p++;
    }

    char message[500];
    va_list args;
    va_start (args, format);
    vsnprintf(message, 500, format, args);
    va_end (args);

    fprintf(stderr, "%s:%d:%d error: %s\n", file, line, (int)(p - lastline), message);
  }
  static const char* file;
  static const char* content;
private:
  bool all_;
  bool inquote_;
};

const char* PhpProcessor::file=0;
const char* PhpProcessor::content=0;

/*
  Simple samples:
    echo $val?>
    echo $val;
    echo 'val'
    echo "val"
  Single line samples::
    echo 'aaa"bbb"ccc"ddd"eee'
    echo "aaa'bbb'ccc'ddd'eee"
    echo 'aaa'.$val.'bbb'
	  echo "aaa\"bbb\"ccc"
    echo "aaa", $val, "bbb";
  Multiline samples:
    echo 'aaa
          bbb
          ccc';
    echo 'aaa
          bbb'.$val.'ccc'
          ddd';
    echo "aaa", $val, ";
         bbb", $val, ";
         ccc";
*/
class EchoProcessor: public PhpProcessor
{
public:
  EchoProcessor()
  {
    quote = 0;
    state = EchoNone;
  }
  virtual void onTagOpen()
  {
    quote = 0;
    state == EchoNone;
  }
  virtual void onTagClose(const char** pps, char** ppr)
  {
    if(state == EchoClose)
    {
      char* pr = *ppr;
      *(pr++) = ')';
      *(pr++) = ';';
      *ppr = pr;
      state = EchoNone;
    }
  }
  virtual void onChar(const char** pps, char** ppr)
  {
    const char* ps = *pps;
    char* pr = *ppr;

    if(state == EchoNone)
    {
      if(ps[0] == 'e'
      && ps[1] == 'c'
      && ps[2] == 'h'
      && ps[3] == 'o')
      {
        ps += 4;

        pr[0] = 'e';
        pr[1] = 'c';
        pr[2] = 'h';
        pr[3] = 'o';
        pr[4] = '(';
        pr += 5;

        state = EchoOpen;
        quote = 0;

        *pps = ps;
        *ppr = pr;
        return;
      }
    }

    if(state == EchoOpen)
    {
      if(!isspace(*ps))
      {
        if(*ps == '\'' || *ps == '\"')
        {
          quote = *ps;
        }
        state = EchoClose;
      }
    }
    else if(state == EchoClose)
    {
      if(!quote )
      {
        if(*ps == ';')
        {
          *(pr++) = ')';
          state = EchoNone;
        }
        else if(ps[0] == '?' && ps[1] == '>')
        {
          *(pr++) = ')';
          *(pr++) = ';';
          state = EchoNone;
        }
      }
      if(state == EchoClose)
      {
        if(*ps == '\'' 
            || ( *ps == '\"'
                  && *(ps-1) != '\\'))/*ignore escaped quotes*/
        {
          if(quote && quote == *ps)
            quote = 0;
          else if(!quote)
            quote = *ps;
        }
        else if(*ps == '\n')
        {
          *(pr++) = '\\';
        }
      }
    }
    *(pr++) = *(ps++);
    *pps = ps;
    *ppr = pr;
  }

  enum EchoState {
    EchoNone,
    EchoOpen,
    EchoClose
  };
  EchoState state;  
  char quote;
};

/*
  handle 3 types of entities that can be concatenated with '.':
    strings (single or double quoted)
    variables that always start with $
    method calls that always have open and closing parenthesis (...)
    
  detect strings and variables and when leaving these, check for a '.'
  if '.' is found replace with + and go into a searching phase to 
  detect strings, variable and now methods, after the '.'

  this means we can detect string.* and variable.* but not method.*
  trying to detect method.* is too complex because we'd have to filter out if(foo) and foreach(foo)
   plus there's few cases of method start a concat.
 */

class ConcatProcessor: public PhpProcessor
{
public:
  ConcatProcessor()
  {
    quote = 0;
    state = StateNone;
  }
  virtual void onTagOpen()
  {
    state = StateNone;
    quote = paren = 0;
  }
  virtual void onChar(const char** pps, char** ppr)
  {
    const char* ps = *pps;
    char* pr = *ppr;

    if(state == StateNone || state == StateConcat)
    {
      if(*ps == '\'' || *ps == '\"')
      {
        setInQuote(true);
        state = StateQuote;
        quote = *ps;
      }
      else if(*ps == '$')
      {
        state = StateVariable;
      }
      else if(state == StateConcat && isalpha(*ps))
      {
        state = StateMethod;
      }
    }
    else if(state == StateQuote)
    {
      /* check for end of quoted string */
      if(quote == *ps 
      && !(*ps == '\"' 
       && *(ps-1) == '\\'))/*ignore escaped quotes*/
      {
        setInQuote(false);
        quote = 0;
      }
      else if(!quote)
      { 
        /*we are past the end quote and looking for the first non-space character being a '.' */
        if(*ps == '.')
        {
          ps++;
          *(pr++) = '+';
          state = StateConcat;
          *pps = ps;
          *ppr = pr;
          return;
        }
        else if(!isspace(*ps))
        {
          state = StateNone;
        }
      }
    }
    else if(state == StateVariable)
    {
      /*check for dot*/
      if(*ps == '.')
      {
        ps++;
        *(pr++) = '+';
        state = StateConcat;
        *pps = ps;
        *ppr = pr;
        return;
      }
      else if(*ps == '[')
      {
        state = StateVariableBracket;
      }
      else if(*ps != '_' && ispunct(*ps))
      {
        state = StateNone;
      }
    }
    else if(state == StateVariableBracket)
    {
      if(*ps == ']')
      {
        state = StateVariable;
      }
    }
    else if(state == StateMethod)
    {
      /*check for closing parens*/
      if(paren == 0 && *ps == '(')
      {
        paren = '(';
      }
      else if(paren == '(' && *ps == ')')
      {
        paren = ')';
      }
      else if(paren == 0 && !isalnum(*ps))
      {
        printError(ps, "in state method found nonalphanumeric");
        state = StateNone;
      }
      else if(paren == ')')
      { /*we are past the closing paren so check for '.' or whitespace*/
        if(*ps == '.')
        {
          ps++;
          *(pr++) = '+';
          state = StateConcat;
          paren = 0;
          *pps = ps;
          *ppr = pr;
          return;
        }
        else if(!isspace(*ps))
        {
          state = StateNone;
          paren = 0;
        }
      }
    }
    *(pr++) = *(ps++);
    *pps = ps;
    *ppr = pr;
  }

  char quote=0;
  char paren=0;
  enum CState
  {
    StateNone,
    StateQuote,
    StateVariable,
    StateVariableBracket,
    StateMethod,
    StateConcat
  };
  CState state;

};

/*
  simply find and replace elseif with else if
 */

class ElseIfProcessor: public PhpProcessor
{
public:
  ElseIfProcessor()
  {
  }
  virtual void onChar(const char** pps, char** ppr)
  {
    if(strncmp(*pps, "elseif", 6) == 0)
    {
      strcpy(*ppr, "else if");
      *pps += 6;
      *ppr += 7;
    }
    else
    {
      *((*ppr)++) = *((*pps)++);
    }
  }
};

/*
  two type of foreach
    foreach ($arr as $value) {
    foreach ($arr as $key => $value) {

  covert to javascript:
    for ($key in $arr) { $value=$arr[$key]
*/
class ForEachProcessor: public PhpProcessor
{
public:
  ForEachProcessor()
  {
  }
  virtual void onChar(const char** pps, char** ppr)
  {
    const char* ps = *pps;
    char* pr = *ppr;
    if(strncmp(ps, "foreach", 7) == 0)
    {
      const char* parenopen;
      const char* array;
      const char* as;
      const char* key;
      const char* equal;
      const char* value;
      const char* parenclose;
      const char* braceopen;
      int i;
      char sarray[100] = {0};
      char skey[100] = {0};
      char svalue[100] = {0};

      /*find the open parentheses*/
      parenopen = ps;
      while(*parenopen != '(')
        parenopen++;

      /*find the array variable after the paren*/
      array = parenopen + 1;
      while(isspace(*array))
        array++;

      /*array should be pointing to array variable*/
      if(*array != '$')
      {
        printError(ps, "foreach not pointing to array variable");
        *((*ppr)++) = *((*pps)++);
        return;
      }

      /*copy array variable into our buffer*/
      i = 0;
      while(!isspace(*array))
        sarray[i++] = *(array++);

      /*find the 'as' thing*/
      as = array + 1;
      while(strncmp(as, "as", 2) != 0)
        as++;

      /*determine if there is a key or not by searching for the "=>"*/
      equal = as + 2;
      do
      {
        if(*equal == ')')
        {
          equal = 0;
          break;
        }
        if(*equal == '=' && *(equal+1) == '>')
        {
          break;
        }
      }while(equal++);

      if(equal) 
      {
        /*find the key, search from where the 'as' was*/
        key = as + 2;
        while(isspace(*key))
          key++;

        if(*key != '$')
        {
          printError(ps, "foreach not pointing to key variable");
          *((*ppr)++) = *((*pps)++);
          return;
        }

        i = 0;
        while(!isspace(*key))
          skey[i++] = *(key++);
//printf("key=%s\n", skey);
        /*value will be past the =>*/
        value = equal + 2;
      }
      else
      {
        /*if there was no => then we are after the 'as'*/
        value = as + 2;
        key = 0;
      }

      /*search for the value*/
      while(isspace(*value))
        value++;

      if(*value != '$') 
      {
        printError(ps, "foreach not pointing to value variable");
        *((*ppr)++) = *((*pps)++);
        return;
      }

      /*copy the value*/
      i = 0;
      while(!isspace(value[i]) && value[i]!=')' )
      {
        svalue[i] = value[i];
        i++;
      }
//printf("val=%s\n", svalue);

      /*now find the ')' */
      parenclose = value + i;
      while(isspace(*parenclose))
        parenclose++;
//printf("parenclose=%s\n", parenclose);

      if(*parenclose != ')')
      {
        printError(ps, "foreach no closing parentheses");
        *((*ppr)++) = *((*pps)++);
        return;
      }

      braceopen = parenclose+1;
//printf("braceopen=%s\n", braceopen);
      
      while(isspace(*braceopen))
        braceopen++;

      bool needclosebrace = false;
      if(*braceopen != '{')
      {
        needclosebrace = true;
      }

      /*if no key then create a unique'ish one*/
      int size;
      if(key)
        size = sprintf(pr, "for(%s in %s) { %s=%s[%s];", skey, sarray, svalue, sarray, skey);
      else
        size = sprintf(pr, "for(var $keytmp in %s) { %s=%s[$keytmp];", sarray, svalue, sarray);

      pr += size;

      addLabel(&pr, "foreach");
      //*(pr++) = '\n';

      if(needclosebrace)
      {
        ps = braceopen;
        do
        {
          *(pr++) = *(ps++);
        }while(*ps != ';');
        *(pr++) = *(ps++);
        //*(pr++) = '\n';
        *(pr++) = '}';
        *pps = ps;
        *ppr = pr;
      }
      else
      {
        ps = braceopen+1;
      }
      *pps = ps;
      *ppr = pr;
    }
    else
    {
      *((*ppr)++) = *((*pps)++);
    }
  }
};

/*
  simply find and replace elseif with else if
 */

class PhpNameProcessor: public PhpProcessor
{
public:
  PhpNameProcessor() : PhpProcessor(true)
  {
  }
  virtual void onChar(const char** pps, char** ppr)
  {
    if(strncmp(*pps, "<?php", 5) == 0)
    {
      strcpy(*ppr, "<?%");
      *pps += 5;
      *ppr += 3;
    }
    else if(strncmp(*pps, ".php", 4) == 0)
    {
      strcpy(*ppr, ".jst");
      *pps += 4;
      *ppr += 4;
    }
    else
    {
      *((*ppr)++) = *((*pps)++);
    }
  }
};

/*
  arrays

  array will begin after whitespace or '='; anything else is not an array
    $a = array(...
    $a =array(...
    return array(...
    func($v,array(...
  but not these:
    group_2D_array($data, ...
    is_array($fields)
    array_merge(...

  most arrays are either a indexed list of strings"
    $a = array('aaa', 'bbb', 'ccc');
  or a string to string key value map:
    $a = array( "k1"	=> "v1",
                "k2"	=> "v2",
                "k3"	=> "v3",
                );
  the webui will sometimes put a comma after the last item, which we need to filter out
  note that keys and values can be single quoted as well.

  less common cases are:
   empty array:
    $a = array();
   array of ints:
    $a = array(1,2);
   array of variables:
    $a = array($a, $b, $c)
    $a = array('k1'=>$a, 'k2'=>$b,)
   array with complex values
    $a array('k1' => (intval($i)==5 || intval($i)==6) ? "_other" : "", ...
   array of arrays
    $a = 
      array (
					array(a,b,c,d),
					array(e,f,g,h),
          ...
				);

  Javascript has indexed arrays or key/value objects

  1) determine if array is indexed or key/value
  2) if array is indexed, then the contents inside the parenthesis can go unchanged (unless there is a subarray)
  3) if array is k/v, then the => must change into :, and the last comma before closing parentheses must be removed
*/
class ArrayProcessor: public PhpProcessor
{
public:
  ArrayProcessor() : PhpProcessor()
  {
  }
  virtual void onChar(const char** pps, char** ppr)
  {
    const char* ps = *pps;
    char* pr = *ppr;
    if(strncmp(ps, "array", 5) == 0)
    {
      const char left = *(ps - 1);
      if(left == ' ' || left == '=' || left == ',' || left == '(')
      {
        const char* right = ps+5;

        while(isspace(*right))
          right++;

        if(*right == '(')
        {
          bool is_object = false;
          const char* parenopen = right;
          const char* parenclose = 0;
          int parens = 1;
          for(;;)
          {
            right++;
            if(*right == ')')
            {
              parens--;
              if(parens == 0)
              {
                parenclose = right;
                break;
              }
            }
            else if(*right == '(')
            {
              parens++;
            }
            else if(!is_object && parens == 1 && right[0] == '=' && right[1] == '>')
            {
              is_object = true;
            }
          }
          if(parenclose)
          {
            *(pr++) = is_object ? '{' : '[';
            right = parenopen;
            int parens = 1;
            char* last_colon = 0;
            char* last_comma = 0;
            while(++right != parenclose)
            {
              if(is_object)
              {
                if(*right == ')')
                  parens--;
                else if(*right == '(')
                  parens++;
                else if(parens == 1)
                {
                  if(right[0] == '=' && right[1] == '>')
                  {
                    last_colon = pr;
                    *(pr++) = ':';
                    right++;
                    continue;
                  }
                  else if(*right == ',')
                  {
                    last_comma = pr;
                  }
                }
              }
              *(pr++) = *right;
            }

            /*scan backward and remove any last comma if errornous*/
            if(last_comma && last_colon && last_comma > last_colon)
              *last_comma = ' ';
            
            *(pr++) = is_object ? '}' : ']';
            addLabel(&pr, "array");

            *pps = parenclose+1;
            *ppr = pr;

            
          }
        }
      }
    }
    *((*ppr)++) = *((*pps)++);
  }
};

class Number2StringProcessor: public PhpProcessor
{
public:
  Number2StringProcessor()
  {
  }
  virtual void onChar(const char** pps, char** ppr)
  {
    const char* ps = *pps;
    char* pr = *ppr;

    if(*ps == '[')
    {
      const char* bracket = ps;
      const char* quote = bracket+1;
      /*next character should be a quote*/
      if(*quote == '\'' || *quote == '\"')
      {
        /*next character being $*/
        const char* variable = quote+1;
        if(*variable == '$')
        {
          /*search for end of variable being quote*/
          const char* quote2 = variable + 1;
          bool valid = true;
          while(*quote2 != *quote)
          {
            if(!isalnum(*quote2))
            {
              valid = false;
              break;
            }
            quote2++;
          }
          if(valid)
          {
            /*next character should be closing bracket*/
            const char* bracket2 = quote2+1;
            if(*bracket2 == ']')
            {
              *(pr++) = '[';
              size_t len = quote2-variable;
              strncpy(pr, variable, len);
              pr += len;
              strcpy(pr, ".toString()");
              pr += 11;
              *(pr++) = ']';
              *ppr = pr;
              *pps = bracket2+1;
              return;
            }
          }
        }
      }
    }
    *((*ppr)++) = *((*pps)++);
  }
};

/*
class IncludeProcessor: public PhpProcessor
{
public:
  IncludeProcessor()
  {
  }
  virtual void onChar(const char** pps, char** ppr)
  {
    if(strncmp(*pps, "include_once", 12) == 0 ||
       strncmp(*pps, "require_once", 12) == 0)
    {
      strcpy(*ppr, "else if");
      *pps += 6;
      *ppr += 7;
    }
    else
    {
      *((*ppr)++) = *((*pps)++);
    }
  }
};
*/
size_t processCode(const char* source, size_t len, char** result, PhpProcessor* processor)
{
  const char* ps = source;
  const char* end = source + len;
  char* pr = *result;
  bool incode = false;
  bool inlinecomment = false;
  bool inblockcomment = false;

  for(;;)
  {
    if(ps == end)
      break;

    if(!processor->all())
    {
      /*check if we are inside code tags or not*/
      if(ps[0] == '<' && ps[1] == '?')
      {
        if(incode)
          PhpProcessor::printError(ps, "bad state open tag when incode");  
        incode = true;
        processor->onTagOpen();
      }
      else if(ps[0] == '?' && ps[1] == '>')
      {
        if(!incode)
          PhpProcessor::printError(ps, "bad state close tag when not incode");  
        incode = false;
        processor->onTagClose(&ps, &pr);
      }

      /*not thing to process if outside of code tags*/
      if(!incode)
      {
        *(pr++) = *(ps++);
        continue;
      }

      if(inlinecomment)
      {
        if(*ps == '\n')
          inlinecomment = false;
        *(pr++) = *(ps++);
        continue;
      }
      else if(inblockcomment)
      {
        if(*ps == '/' && *(ps-1) == '*')
          inblockcomment = false;
        *(pr++) = *(ps++);
        continue;
      }
      else if(!processor->inQuote())
      {
        if(*ps == '/' && *(ps-1) == '/')
        {
          inlinecomment = true;
          *(pr++) = *(ps++);
          continue;
        }
        else if(*ps == '*' && *(ps-1) == '/')
        {
          inblockcomment = true;
          *(pr++) = *(ps++);
          continue;
        }
      }
    }

    processor->onChar(&ps, &pr);
  }
  return pr - *result;
}

bool processFile(const string& file)
{
  char* content;
  size_t size;

  if(!readFile(file.c_str(), &content, &size))
    return false;

  char* result1 = new char[size*2];//hack - should be more then enough
  char* result2 = new char[size*2];//hack - should be more then enough

  size_t len = size;

  PhpProcessor::file = file.c_str();
  PhpProcessor::content = content;

  ConcatProcessor concat;
  memset(result1, 0, size);
  len = processCode(content, len , &result1, &concat);

  PhpProcessor::content = result1;
  EchoProcessor echo;
  memset(result2, 0, size);
  len = processCode(result1, len, &result2, &echo);

  PhpProcessor::content = result2;
  ElseIfProcessor elseif;
  memset(result1, 0, size);
  len = processCode(result2, len, &result1, &elseif);

  PhpProcessor::content = result1;
  ForEachProcessor foreach;
  memset(result2, 0, size);
  len = processCode(result1, len, &result2, &foreach);

  PhpProcessor::content = result2;
  PhpNameProcessor phpname;
  memset(result1, 0, size);
  len = processCode(result2, len, &result1, &phpname);

  PhpProcessor::content = result1;  
  ArrayProcessor arrays;
  memset(result2, 0, size);
  len = processCode(result1, len, &result2, &arrays);

  PhpProcessor::content = result2;
  Number2StringProcessor num2str;
  memset(result1, 0, size);
  len = processCode(result2, len, &result1, &num2str);

  string newfile = file;
  if(file.substr(file.length() - 4) != ".jst")
    newfile = file.substr(0, file.length() - 3) + "jst";

  //unlink(file.c_str());  
  writeFile(newfile.c_str(), result1, len);

  delete content;
  delete result1;
  delete result2;
}

void printHelp()
{
  printf("php2jst: convert a file from php to jst\n"); 
  printf("usage php2jst [OPTIONS] [EXTENSION] PATH\n");
  printf(" OPTIONS:\n");
  printf("   -r --recurse recurse through sub directories\n");
  printf(" EXTENSION of the files to process\n");
  printf("   defaults to php\n");
  printf("   these will be renamed to jst\n");
  printf(" PATH the path to start searching from\n");
}

int main(int argc, char* argv[])
{
  string path = "./";
  vector<string> files;
  string match;
  bool recurse = false;

  int option_index;
  static struct option long_options[] = 
  {
    {"recurse", no_argument, 0, 'r'},
    {0, 0, 0, 0}
  };

  while (true)
  {
    int c = getopt_long(argc, argv, "r", long_options, &option_index);
    if (c == -1)
      break;
    switch (c)
    {
      case 'r':
      recurse = true;
      break;
      default:
      printHelp();
      break;
    }
  }

  if(argc == optind+2)
  {
    match = argv[argc-2];
    path = argv[argc-1];
  }
  if(argc == optind+1)
  {
    path = argv[argc-1];
  }
  printf("recurse=%d match=%s path=%s\n", recurse, match.c_str(), path.c_str());


  if(!getFilesInDirectory(path, files, match, recurse))
  {
    printf("failed to read directory\n");
    return 1;
  }

  printf("found %lu files\n", files.size());
  for(auto& i: files)
  {
    printf("%s\n", i.c_str());
    processFile(i);
  }
}
