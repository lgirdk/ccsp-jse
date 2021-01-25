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

try
{
/* HEADERS: accumulate headers into a buffer
            to send to stdout in _jst_finish*/
_jst_header_buffer = "Content-type: text/html";
function header(str)
{
  if(str.toLowerCase().indexOf('location:') == 0)
  {
    _jst_header_buffer = "HTTP/1.0 302 Ok\r\n";
    _jst_header_buffer += "Status: 302 Moved\r\n";
    _jst_header_buffer += str;
  }
  else
  {
    _jst_header_buffer += "\n" + str;
  }
}

/* ECHO: accumulate main content into a buffer
         to send to stdout in _jst_finish */
_jst_echo_buffer = "";
function echo(str)
{
  _jst_echo_buffer += str;
}

/* FINISH: _jst_finish is called at the very end of the script 
           and it will send the headers and content to stdout */
function _jst_finish()
{
  print(_jst_header_buffer);
  print("\r\n\r\n\n");
  print(_jst_echo_buffer);
}

/* EXIT: there is no way to simply quit in the middle of a script, so
         we throw an exception which will be caught in ccsp_builtin_suffix.js
         and that will call _jst_finish to write our content */
function _jst_exit_exception(code)
{
  this._jst_exit_code = code;
}
function exit(code)
{
  if(typeof(code) !== 'number')
    code = 0;
  throw new _jst_exit_exception(code);
}

/* SERVER: web server parameters past to cgi as environment variables */
var $_SERVER = new Proxy({}, {
  get: function(obj, prop){
    var value = ccsp.getenv(prop);
    if(value === false)
      value = undefined;//set undefined so isset will not return true
    return value;
  }
});

/* SESSION: session data set by web app, saved to disk, and referenced by session id stored in cookie */
var $_SESSION = {};
var $_jst_session = null;
function session_start()
{
  if($_jst_session)
    return;
  ccsp_session.start();
  header("Set-Cookie: DUKSID=" + ccsp_session.getId() + ";");
  $_jst_session = ccsp_session.getData();
  $_SESSION = new Proxy($_jst_session, {
    get: function(obj, prop) {
      return obj[prop];
    },
    set: function(obj, prop, val){
      obj[prop] = val;
      ccsp_session.setData(obj);
      return true;
    },
    deleteProperty(obj, prop) {
      if(prop in obj)
      {
        delete obj[prop];
        ccsp_session.setData(obj);
      }
      return true;
    }
  });
}
function session_id()
{
  return ccsp_session.getId();
}
function session_status()
{
  return ccsp_session.getStatus();
}
function session_destroy()
{
  delete $_jst_session;
  $_jst_session = null;
  delete $_SESSION;
  $_SESSION = {};
  return ccsp_session.destroy();
}
function session_unset()
{//FIXME
}
function session_print()
{
  for($k in $_jst_session)
    print($k + "=" + $_jst_session[$k]);
}

/* POST: post data sent in via stdin */
$_POST={};
var postData = ccsp_post.getPost();
if(postData)
{
  var postValues = postData.split('&');
  for(var i = 0; i < postValues.length; ++i)
  {
    var postValue = postValues[i].split('=');
    if(postValue.length == 2)
    {
      var value = postValue[1].replace(/[+]/g," ");
      $_POST[postValue[0]] = decodeURIComponent(value);
    }
    else
      print("unexpected post data");
  }
}

/* GET: query parameters */
$_GET= (function ()
{
  var out = {};
  var qs = $_SERVER["QUERY_STRING"];
  if(qs)
  {
    var ar = qs.split('&');
    for(var i=0; i<ar.length; ++i)
    {
      var ar2 = ar[i].split('=');
      if(ar2.length != 2)
        throw Error("$_GET: Invalid QUERY_STRING");
      out[ar2[0]] = ar2[1];
    }
  }
  return out;
})();

function include($filepath)
{
  ccsp.include($filepath);
}

/* begin application code */

