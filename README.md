# README

## Introduction

JSE is a JavaScript interpreter for running command line scripts and CGI
scripts.

## Features

 * Built in JavaScript **Request** object has properties for each of the
CGI environment variables
 * The **Request** object has a **QueryParameters** property, an object
that comprises each of the HTTP request parameters
 * The **QueryParameters** object has properties that comprise each of
the HTTP cookies in the request
 * The **QueryParameters** object has properties that comprise each of
the uploaded files
 * Generation of XML documents from JavaScript objects may be enabled
 * Encryption and decryption of JavaScript strings and buffers may be
enabled
 * Fast CGI support may be enabled

## Dependencies

JSE requires the following libraries:

 * [duktape](https://duktape.org/)
 * [qdecoder](https://github.com/MelanieRed/qdecoder)
 * [libxml2](http://xmlsoft.org/) - optional
 * [libcrypto](https://www.openssl.org/) - optional
 * [fastcgi](https://fastcgi-archives.github.io/) - optional

## Configuring

The build needs to be configured using [CMake](https://cmake.org/) with a
version of at least 3.8. Create a build directory in the same directory as
you have unpacked the JSE source.


## Build options

Build options can be enabled using the CMake -D switch for example:
```
$ cmake ../jse -DENABLE_LIBXML2=ON -DENABLE_LIBCRYPTO=ON
```

The following build options are available:

Option | Default value | Alternative | Description
-------|---------------|-------------|------------
BUILD_RDK | OFF | ON | Build with Cosa CCSP API
FAST_CGI | OFF | ON | Enable Fast CGI support
ENABLE_LIBXML2 | OFF | ON | Enable XML generation API
ENABLE_LIBCRYPTO | OFF | ON | Enable encryption/decryption API

## Building

Once configured JSE can be built using make.

```
$ make
```


## Command syntax

The command syntax is:

`jse [options] [script_name]`

 * In non HTTP mode the script name is optional. If it is omitted the script
is read from standard input
 * In HTTP CGI mode the script name is required
 * In HTTP Fast CGI mode the script name is ignored. The script name is
provided via the SCRIPT_FILENAME environment variable as part of the
**Request** object.

The following command line options are available:

Option | Long Option | Description
-------|-------------|------------
 -c | --cookies | Process HTTP cookies
 -e | --enter-exit | Enable function enter/exit debug
 -g | --get | Process HTTP GET requests
 -h | --help | Help
 -n | --no-ccsp | Do not initialise CCSP (when built in)
 -p | --post | Process HTTP POST requests
 -u | --upload-dir | Specify a different HTTP file upload directory (default /var/jse/uploads)
 -v | --verbose | Verbosity. Use multiple times to turn up verbosity

