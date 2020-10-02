# API Guide

## Built in objects

### Request

The **Request** object is created when JSE is in HTTP mode. I.e. --cookie,
--get or --post is used. It is a JavaScript object with the following
properties:

Name | Description
-----|------------
DOCUMENT_ROOT | The root directory of the web server
HTTP_COOKIE | The visitor cookie, if set. (See also getCookie())
HTTP_REFERER | The URL of the page that called the script. (Note that the typo in the name is per the standard - blame Sir Tim)
HTTP_USER_AGENT	| The browser type of the visitor
HTTPS | "on" if the program is being called through a secure server
PATH | The system path your server is running under
QUERY_STRING | The query string (Everything after the ? in the URI. This is parsed and the individual parameters are supplied as properties in **Request**)
REMOTE_ADDR | The IP address of the visitor
REMOTE_HOST | The hostname of the visitor (if your server has reverse-name-lookups on; otherwise this is the IP address again)
REMOTE_PORT | The port the visitor is connected to on the web server
REMOTE_USER | The visitor's username (for .htaccess-protected pages)
REQUEST_METHOD | GET, POST, PUT etc.
REQUEST_URI | The interpreted pathname of the requested document or CGI (relative to the document root)
SCRIPT_FILENAME | The full pathname of the current CGI
SCRIPT_NAME | The interpreted pathname of the current CGI (relative to the document root)
QueryParameters | The HTTP query parameters

#### QueryParameters property

The **QueryParameters** property comprises the HTTP parameters passed as part
of the HTTP request. These are either taken from the QUERY_STRING environment
variable, in the case of an HTTP GET, or from the body of the Request in an
"application/x-www-form-urlencoded" encoding.

##### Example

_request.js_

```javascript
// Iterate through the properties in Request
for (property in Request) {
    // If the value of a property is an object
    if (typeof Request[property] == "object") {
        // Iterate through the properties in that object
        for (subProperty in Request[property]) {
            // Print the values. Properties can be referred to using either object[property]
            // or object.property
            print(property + "." + subProperty + " = \"" + Request[property][subProperty] + "\"\n");
        }
    } else {
        print(property + " = \"" + Request[property] + "\"\n");
    }
}
```

_Execution_

```
$ export QUERY_STRING=name=Melanie
$ jse -g request.js
Status: 200
Content-Type: text/plain
 
QueryParameters.name = "Melanie"
QUERY_STRING = "name=Melanie"
```

#### QueryParameters file properties

If a file input tag, of the type `<input type="file" name="myUpload">`, is
part of the form then the data will be "multipart/form-data". The 
**QueryParameters** property will have the name specified in the input tag, 
e.g. "myUpload" in this case, and will comprise an object with the following
properties:

Name | Description
-----|------------
filename | The name of the file selected using the `<input type="file">` tag in the browser
contenttype | The file content type
savepath | The file saved on the server's file system (typically a generated name in upload-dir)
length | The file size in bytes

##### File upload example

The following script, if saved as **upload.js**, will display an upload form
on a GET and will display the contents of the file uploaded with a POST.

_upload.js_

```javascript
if (Request.REQUEST_METHOD == "GET") {
    print(
        '<html>' +
        '<head><title>Upload test</title></head>' +
        '<body>' +
        '<h1>File upload</h1>' +
        '<form action="/xml/upload.js" method="post" enctype="multipart/form-data">' +
        '<input type="file" name="upload" />' +
        '<input type="submit" name="submit" />' +
        '</form>' +
        '</body>'
    );
    setContentType("text/html");
} else
if (Request.REQUEST_METHOD == "POST") {
    var fileContent = readFileAsString(Request.QueryParameters.upload.savepath);
 
    print(fileContent);
    print("\r\n");
 
    setContentType("text/plain");
}
```

## Built in errors

### PosixError

PosixError is a JavaScript Error object with the name "PosixError". It has
an additional property "errno" that comprises the POSIX errno of the
underlying error.

### CosaError (Enabled by the BUILD_RDK option)

CosaError is a JavaScript Error object with the name "CosaError". It has
an additional property "errorCode" that comprises the Cosa error code of
the underlying error.

## Built in functions

### Script processing

#### include(string:path)

Includes another JavaScript script file.

##### Arguments

Type | Description
-----|------------
string | The path of the script to include

##### Description

When the file is included it is read, interpreted and its functions and
variables are added to the global context. I.e. it behaves as if the
**include()** line is replaced by the included file.

If **path** does not exist, or cannot be read, a PosixError is thrown
with an appropriate errno value e.g. ENOENT or EPERM etc.

### Output

#### print(any:value)

Outputs or prints the value to standard output.

##### Argument

Type | Description
-----|------------
any | The message to output

##### Description

Outputs or prints the message to standard output using the value's
toString() method where necessary.

If the script is running as a CGI script then the output does not occur
until all HTTP response headers have been output first. So output may be
print()ed before headers are set. All output occurs in such circumstances
when the script concludes.

Print does not end the line with a linefeed or carriage return!

#### debugPrint(any:value, number:level[optional])

Outputs or prints the value to the debug stream.

##### Arguments

Type | Description
-----|------------
any | The message to output
number | The debug level of the output

##### Description

Outputs or prints the value to the debug system with the specified debug
level. The value's toString() method is used if necessary.

If the debug level is omitted the error level is used. Typically
this outputs to the standard error stream which, for HTTP requests, is the
HTTP server's error logs.

### HTTP response methods

HTTP response methods are only relevant when JSE is in HTTP mode.

#### setHTTPStatus(number:code)

Sets the HTTP status code for the HTTP response.

##### Argument

Type | Description
-----|------------
number | The status code

##### Description

Sets the HTTP status code for the HTTP response. This will be output first
as the top line of the HTTP response.

##### Example

_status.js_

```javascript
setHTTPStatus(418);
print("I am a teapot!\n");
```

_Execution_

```
$ jse -g status.js
Status: 418 I'm a teapot
Content-Type: text/plain

I am a teapot!
```

#### setContentType(string:contentType)

Sets the content type of the body of the HTTP response.

##### Argument

Type | Description
-----|------------
string | The content type

##### Description

Sets the HTTP content type for the HTTP response. I.e. it sets the 
"Content-Type" header.

##### Example

_content.js_

```javascript
setContentType("text/html");
print("<html><head><title>Hello</title></head><body><h1><Hello</h1></body></html>\n");
```

_Execution_

```
$ jse -g content.js
Status: 200
Content-Type: text/html
 
<html><head><title>Hello</title></head><body><h1><Hello</h1></body></html>
```

#### setHeader(string:name, string:value)

Sets an HTTP response header.

##### Arguments

Type | Description
-----|------------
string | The header name
string | The header value

##### Description

Sets an HTTP header value. If the header does not exist a new one is added.
If it does exist its value is replaced. The validity of the header name is 
NOT checked.

##### Example

_header.js_

```javascript
setHeader('X-My-Special-Header', 'Some special stuff');
setHeader('Content-Length', "17");
print('Headers are set!\n');
```

_Execution_

```
$ jse -g header.js
Status: 200
X-My-Special-Header: Some special stuff
Content-Length: 17
Content-Type: text/plain
 
Headers are set!
```

#### setCookie(string:name, string:value, number:expiry[optional], string:path[optional], string:domain[optional], boolean:secure[optional])

Sets an HTTP cookie value.

##### Arguments

Type | Description
-----|------------
string | The cookie name.
string | The cookie value.
number | The expiry time in seconds (optional - defaults to forever)
string | The cookie path (optional or may be **null**).
string | The cookie domain (optional or may be **null**).
boolean | Set true if a secure cookie (optional - defaults to false)


##### Description

Sets a cookie. This is used as the "Set-Cookie:" field in the header of the HTTP response.

##### Example

_cookie.js_

```javascript
setCookie('name', 'melanie', 3600, '/home/melanie', null, false);
print('Cookie is set!\n');
```

_Execution_

```
$ jse -g cookie.js
Status: 200
Set-Cookie: name=melanie; expires=Tue, 05 May 2020 16:01:12 GMT; path=/home/melanie
Content-Type: text/plain
 
Cookie is set!
```

### File I/O

#### writeAsFile(string:path, any:value, boolean:create)

Writes the value as a file.

##### Arguments

Type | Description
-----|------------
string | The file path.
any | The value.
boolean | Create file (optional - defaults to **false**)


##### Description

Writes the value as the contents of the specified file, optionally creating
the file if it does not exist. It uses the value's **toString()** method to
coerce it in to a string.

The function throws a URIError if the path is invalid for some reason.

##### Example

_write_file.js_

```javascript
var pi = 3.1415926;
writeAsFile("test.txt", pi, true);
```

_Execution_

```
$ jse write_file.js
$ cat test.txt
3.1415926
```

#### readFileAsString(string:path)

Reads the contents of a file returning it as a string.

##### Argument

Type | Description
-----|------------
string | The file path.

##### Description

Reads the contents of the specified file returning it as a single string.

##### Example

_read_file.js_

```javascript
var str = readFileAsString("/proc/cmdline");
print(str);
```

_Execution_

```
$ jse read_file.js
BOOT_IMAGE=/vmlinuz-4.9.0-12-amd64 root=/dev/mapper/C--ML--N240WU--vg-root ro quiet
```

#### readFileAsBuffer(string: path)

Reads the contents of a file in to an Array Buffer.

##### Argument

Type | Description
-----|------------
string | The file path.

##### Description

This is similar to **readFileAsString()** except it reads the contents of the
specified file returning it as a Uint8Array.

#### removeFile(string:path)

Unlinks the specified file or directory, deleting it when it is no longer used.

##### Argument

Type | Description
-----|------------
string | The file path.

#### createDirectory(string:path)

Creates a new directory at the specified path.

##### Argument

Type | Description
-----|------------
string | The directory path.

#### listDirectory(string:path)

Returns a file list of the specified directory.

##### Argument

Type | Description
-----|------------
string | The directory path.

##### Description

This function returns an array of strings, each string being the name of a
file in the specified directory.

##### Example

_list_directory.js_

```javascript
var files = listDirectory("/proc");
for (var index in files) {
        print(files[index] + "\n");
}
```

_Execution_

```
$ jse list_directory.js
.
..
fb
fs
bus
dma
irq
(etc)
```

### Time functions

#### sleep(number:sec)

Sleeps for **sec** seconds

###### Argument

Type | Description
-----|------------
number | The delay in seconds.

#### usleep(number:usec)

Sleeps for **usec** microseconds

###### Argument

Type | Description
-----|------------
number | The delay in microseconds.

### Process control

#### execProcess(string:path, array:arguments[optional], object:environment[optional])

Spawns a new sub-process.

##### Arguments

Type | Description
-----|------------
string | The full path and filename of the executable to be run
array | Array of strings comprising the arguments to the executable (optional)
object | An object comprising properties that become part of the environment for the executable (optional)

##### Description

This function spawns a new sub-process. The JavaScript thread waits until
the sub-process completes. The process does NOT run in a shell. It is
started using fork() followed by execvp(). If you require a shell, start
the shell using **execProcess()** and pass the the process to run via the
arguments of that shell.

The arguments array comprises an array of arguments that are passed to the
processed being executed. The first argument in the array is not, as is
typical, the process name.

The properties of the environment object are used to specify environment
variables passed to the process. The property name becomes the variable
name and its value, coerced to a string, is set as the environment
variable's value.

On completion the function returns an object with the following properties:

Name | Description
-----|------------
pid | The process id
status | The process' exit status
stdout | A string comprising the data written to standard output
stderr | A string comprising the data written to standard error

##### Example

_process.js_

```javascript
var results = execProcess("/bin/date");
 
print("results.pid=" + results.pid + "\n");
print("results.status=" + results.status + "\n");
print("results.stdout=" + results.stdout + "\n");
print("results.stderr=" + results.stderr + "\n");
```

_Execution_

```
$ process.js
results.pid=30486
results.status=0
results.stdout=Fri Jun 19 15:15:05 BST 2020
 
results.stderr=
```

#### sendSignal(number:pid, number:signal)

Sends the specified signal to the specified process.

##### Arguments

Type | Description
-----|------------
number | The process id
number | The signal

### XML functions

#### objectToXMLString(string:rootName, any:value)

Returns an XML document that describes the specified value.

##### Arguments

Type | Description
-----|------------
string | The root entity name
any | The value to serialise

##### Description

This function returns a string comprising an XML document. Boolean, number
and string values are used as the value of the root entity. Arrays and
objects result in sub-entities. In the case of an array, multiple entities
with the same name, one for each element. And for objects, the entities
represent each property of the object. 

##### Example

_xml.js_

```javascript
var myObject = {
    name: "Eric",
    age: 69,
    height: 175,
    skills: [
        "C", "C++", "Java", "JavaScript"
    ],
        address: {
        house: 69,
        street: "Acacia Avenue",
        city: "Anywhere",
        country: "UK"
    }
};
 
print(objectToXMLString("person", myObject));
```

_execution_

```
$ jse xml.js
<?xml version="1.0"?>
<person>
  <name>Eric</name>
  <age>69</age>
  <height>175</height>
  <skills>C</skills>
  <skills>C++</skills>
  <skills>Java</skills>
  <skills>JavaScript</skills>
  <address>
    <house>69</house>
    <street>Acacia Avenue</street>
    <city>Anywhere</city>
    <country>UK</country>
  </address>
</person>
```

### Encryption/Decryption functions

#### encrypt(string/buffer:data, string/buffer:key, number:method, string/buffer:iv[optional])

Encrypts the data using the method and the key specified. The method may
require an initialisation vector which can also be specified.

##### Arguments

Type | Description
-----|------------
string/buffer | The data to encrypt
string/buffer | The encryption key
number | The encryption method
string/buffer | The initialisation vector (optional depending upon encryption method)

##### Description

This function encrypts the data supplied in the string or array buffer using
the specified key, encryption method and initialisation vector if required.
It returns a Uint8Array object comprising the encrypted data. The key and
initialisation vector may be specified as a string or as an array buffer 
typically a Uint8Array.

The encryption methods are as follows:

Number | Description
-------|------------
0 | AES 128 bit CBC
1 | AES 128 bit ECB
2 | AES 128 bit CFB
3 | AES 128 bit OFB
4 | AES 192 bit CBC
5 | AES 192 bit ECB
6 | AES 192 bit CFB
7 | AES 192 bit OFB
8 | AES 256 bit CBC
9 | AES 256 bit ECB
10 | AES 256 bit CFB
11 | AES 256 bit OFB
12 | DES CBC
13 | DES ECB
14 | DES CFB
15 | DES OFB
16 | DES EDE CBC
17 | DES EDE ECB
18 | DES EDE CFB
19 | DES EDE OFB
20 | Triple DES EDE CBC
21 | Triple DES EDE ECB
22 | Triple DES EDE CFB
23 | Triple DES EDE OFB

#### decryptAsString(buffer:data, string/buffer:key, number:method, string/buffer:iv[optional])

Decrypts the data using the method and the key specified. The method may
require an initialisation vector which can also be specified. The 
unencrypted data is returned as a string.

##### Arguments

Type | Description
-----|------------
buffer | The data to decrypt
string/buffer | The decryption key
number | The decryption method
string/buffer | The initialisation vector (optional depending upon decryption method)

##### Description

This function decrypts the data supplied in the array buffer using the
specified key, encryption method and initialisation vector if required.
It returns a string comprising the decrypted data. The key and
initialisation vector may be specified as a string or as an array buffer 
typically a Uint8Array.

#### decryptAsBuffer(buffer:data, string/buffer:key, number:method, string/buffer:iv[optional])

Decrypts the data using the method and the key specified. The method may
require an initialisation vector which can also be specified. The 
unencrypted data is returned as a Uint8Array.

##### Arguments

Type | Description
-----|------------
buffer | The data to decrypt
string/buffer | The decryption key
number | The decryption method
string/buffer | The initialisation vector (optional depending upon decryption method)

##### Description

This function decrypts the data supplied in the array buffer using the
specified key, encryption method and initialisation vector if required.
It returns a Uint8Array comprising the decrypted data. The key and
initialisation vector may be specified as a string or as an array buffer 
typically a Uint8Array.

### Cosa CCSP functions

The following functions are read and write CCSP keys. They are all part of
the Cosa object and so should be prefixed with *Cosa.*.

#### getStr(string:name)

Returns, as a string, the value of the key with the specified name.

##### Arguments

Type | Description
-----|------------
string | The CCSP key name

#### setStr(string:name, any:value, boolean:commit)

Sets a CCSP key to the specified value, committing it if commit is *true*.

##### Arguments

Type | Description
-----|------------
string | The CCSP key name
any | The value (see description)
boolean | The commit flag

##### Description

This function sets a CCSP key to the specified value. The value can be any
JavaScript type but it will be cast or coerced to a string. This uses the
values *toString()* method which can result in unexpected values so don't
use if you are unsure of how the value converts. In those cases it is 
suggested you explicitly cast to a string in the calling code.

The commit flag specifies whether the value should be committed to the 
underlying data model. This allows keys to be set and then all the values
committed in one go with the final value.

#### getInstanceIds(string:name)

Returns, as a string, a comma separated list of the IDs of instances.

##### Arguments

Type | Description
-----|------------
string | The CCSP root object name

##### Description

This function returns a string comprising a comma separated list of the
instances of the specified root object.

This function is deprecated and it is suggested that *DmExtGetInstanceIds()*
is used instead (See below).

#### addTblObj(string:name)

#### delTblObj(string:name)

#### DmExtGetInstanceIds(string:name)

Returns, as an array, a list of the IDs of instances.

##### Arguments

Type | Description
-----|------------
string | The CCSP root object name

##### Description

This function returns an array comprising a list of the instances of the
specified root object. The first element of the array is the return status
from the underlying C calls. This is for backwards compatibility with old
users of the Cosa API. Since this call throws a CosaError on error this
value will always be zero.

#### DmExtGetStrsWithRootObj(string:name, array:fields)

Returns a number of object field values as an array.

##### Arguments

Type | Description
-----|------------
string | The CCSP root object name
array | An array of strings of full field names for the object

##### Description

This function will return an array of one or more values as strings for
the fields specified. Each element in the array will have a index and
the value will be a comma separated pair of the field name and value.
The first element of the array is the return status from the underlying
C calls. This is for backwards compatibility with old users of the Cosa
API. Since this call throws a CosaError on error this value will always
be zero.

The field name MUST be the full field path name and have the same root
as the specified root object.

##### Example

_dmextgetstrswithrootobj.js_

```javascript
var values = Cosa.DmExtGetStrsWithRootObj(
    "Device.Hosts.Host.1.", [
        "Device.Hosts.Host.1.PhysAddress",
        "Device.Hosts.Host.1.IPAddress",
        "Device.Hosts.Host.1.HostName"
    ]);

for (var key in values) {
    print(key + "=" + values[key] + "\n");
}

```

_Execution_

```
$ jse dmextgetstrswithrootobj.js
0=0
1=Device.Hosts.Host.1.PhysAddress,00:11:22:33:44:55
2=Device.Hosts.Host.1.IPAddress,10.0.0.100
3=Device.Hosts.Host.1.HostName,MyLaptop
```

#### DmExtSetStrsWithRootObj(string:name, boolean:commit, array:fieldData)

Sets a number of field values for a root object

##### Arguments

Type | Description
-----|------------
string | The CCSP root object name
boolean | The commit flag
array | An array of triplets in arrays

##### Description

This function will set the value of one or more fields for the specified
root object. The fields are specified as sub arrays with three elements
which are as follows

Index | Type | Description
------|------|------------
0 | string | Full field name
1 | string | CCSP type
2 | any | The value (see below)

The field name MUST be the full field path name and have the same root
as the specified root object.

The CCSP type is the type of the CCSP key.

The value can be any JavaScript type but it will be cast or coerced to
a string. This uses the values *toString()* method which can result in
unexpected values so don't use if you are unsure of how the value
converts. In those cases it is suggested you explicitly cast to a string
in the calling code.

##### Example

_dmextsetstrswithrootobj.js_

```javascript
Cosa.DmExtSetStrsWithRootObj(
    "Device.Hosts.Host.1.", true, [
        [ "Device.Hosts.Host.1.PhysAddress", "string", "00:11:22:33:44:55" ],
        [ "Device.Hosts.Host.1.IPAddress", "string", "10.0.0.55" ],
        [ "Device.Hosts.Host.1.HostName", "string", "myHostName" ]
    ]);
```


