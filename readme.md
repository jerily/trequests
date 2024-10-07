# trequests

TCL/C extension that provides an interface to the [libcurl](https://curl.se/libcurl/) C library. It contains functions for making synchronous and asynchronous HTTP/HTTPS requests, specifying parameters for GET/POST request types, working with response data and creating sessions in which requests share parameters, TCP connections, cookies.

## Requirements

- [libcurl](https://curl.se/libcurl/)

Supported systems:

- Linux
- MacOS

## Installation

### Install Dependencies

Make sure that the build environment has [cURL development packages](https://curl.se/download.html). For example, on Ubuntu Linux, the development package can be installed using the following commands:

```bash
sudo apt update
sudo apt install -y libcurl4-openssl-dev
```

or build [cURL](https://github.com/curl/curl) with dependencies using sources:

#### Install OpenSSL

```bash
curl -sL https://github.com/openssl/openssl/releases/download/openssl-3.3.2/openssl-3.3.2.tar.gz | tar zx
cd openssl-*
./Configure
make
make install
```

#### Install cURL

```bash
curl -sL https://github.com/curl/curl/releases/download/curl-8_9_1/curl-8.9.1.tar.gz | tar zx
cd curl-*
./configure --with-openssl
make
make install
```

### Install trequests

```bash
git clone https://github.com/jerily/trequests.git
cd trequests
mkdir build
cd build
cmake ..
# or if TCL is not in the default path (/usr/local/lib):
# change "TCL_LIBRARY_DIR" and "TCL_INCLUDE_DIR" to the correct paths
# cmake .. -DTCL_LIBRARY_DIR=/usr/local/lib -DTCL_INCLUDE_DIR=/usr/local/include
make
make install
```

## Usage

### Non-session requests

The following commands are available to create HTTP/HTTPS requests:

* **::trequests::head url ?options?** - creates HEAD request
* **::trequests::get url ?options?** - creates GET request
* **::trequests::post url ?options?** - creates POST request
* **::trequests::put url ?options?** - creates PUT request
* **::trequests::patch url ?option?** - creates PATCH request
* **::trequests::delete url ?options?** - creates DELETE request
* **::trequests::request method url ?options?** - creates a custom request using the specified HTTP method

All these commands are synchronous by default. They make an HTTP/HTTPS request using the provided URL and parameters and return a response handle that can be used to get information about the response or error message if one occurred. Their behavior may differ if the switches **-async** or **-simple** are specified in **?options?**.

### Request options

#### Async/basic requests

* **-async** - specifies asynchronous request. See the section below for details on asynchronous requests.
* **-callback command** - specifies a callback for asynchronous request. See the section below for details on asynchronous requests.
* **-simple** - specifies simple request. In this case a request commands returns not response handle, but directly the data returned by the web server.

#### Base request options

* **-headers headers** - a value in key-value (dictionary) format that specifies custom HTTP headers. Can be specified multiple times. In this case, the dictionaries will be merged.
* **-accept value** - specifies a value for the `Accept:` HTTP header. Can take the value `json`, which is a shortcut for `application/json`.
* **-content_type value** - specifies a value for the `Content-Type:` HTTP header. Can take the value `json`, which is a shortcut for `application/json`.
* **-allow_redirects boolean** - allows or disallows redirect following (default is: `true`)
* **-timeout milliseconds** - timeout in milliseconds for the entire request
* **-timeout_connect milliseconds** - timeout in milliseconds for the request connection phase
* **-verify_host boolean** - specifies whether the server's certificate's claimed identity must be verified (default is: `true`)
* **-verify_peer boolean** - specifies whether the authenticity of the peer's certificate must be verified (default is: `true`)
* **-verify boolean** - this is a shortcut for specifying both **-verify_host** and **-verify_peer** options
* **-verify_status boolean** - specifies whether the status of the server certificate using the "Certificate Status Request" TLS extension must be verified (default is: `false`)

#### Authentication options

* **-auth username_password** - specifies a list of 2 elements **username** and **password**. These credentials will be used for Basic Authentication.
* **-auth_token token** - specifies a token which will be used in Bearer Authentication.
* **-auth_scheme scheme** - specifies the authentication schemes that can be used. Accepts a list with 1 or more elements of the following authentication scheme names: `basic`, `digest`, `digest_ie`, `bearer`, `negotiate`, `ntlm`, `aws_sigv4`. It can also take a single value of `any` or `anysafe` that corresponds to any authentication scheme or authentication schemes that are considered secure.
* **-auth_aws_sigv4 list** - specifies the parameters for AWS V4 signature authentication. Accepts a number of list elements from 1 to 4, which will be concatenated with a colon and passed to the cURL parameter [CURLOPT_AWS_SIGV4](https://curl.se/libcurl/c/CURLOPT_AWS_SIGV4.html).

#### URL query parameters

* **-params data** - takes an even list of elements, which are treated as key-value pairs. Each key and value will be urlencoded and concatenated with an equals symbol (`=`). All key-value pairs will be concatenated with an ampersand character (`&`). Can be specified multiple times.
* **-params_raw data** - accepts a string that will be used in the URL request parameters as is. Can be specified multiple times. All strings specified by this parameter will be concatenated with an ampersand character (`&`).

#### POST parameters

* **-form form_data** - a value in key-value (dictionary) format that specifies form POST data. Can be specified multiple times. In this case, the dictionaries will be merged.
* **-data data** - accepts a string that will be used as POST data as is. Can be specified multiple times. All strings specified by this parameter will be concatenated with an ampersand character (`&`).
* **-data_urlencode data** - accepts a string that will be urlencoded and used as POST data. Can be specified multiple times. All strings specified by this parameter will be concatenated with an ampersand character (`&`).
* **-data_fields data** - takes an even list of elements, which are treated as key-value pairs. Each key and value will be concatenated with an equals symbol (`=`). All key-value pairs will be concatenated with an ampersand character (`&`). Can be specified multiple times.
* **-data_fields_urlencode data** - takes an even list of elements, which are treated as key-value pairs. Each key and value will be urlencoded and concatenated with an equals symbol (`=`). All key-value pairs will be concatenated with an ampersand character (`&`). Can be specified multiple times.
* **-json data** - accepts a string that will be used as POST data as is and also it sets `Accepts:` and `Content-Type:` HTTP headers to JSON format. This is a shortcut to a set of options: `-data <data> -accept json -content_type json`. The difference with the **-data** option is that this option can only be specified once.

#### Debugging parameters

* **-verbose boolean** - enables or disables verbose debug messages during a request. If enabled and **-callback_debug** options is not specified, debug messages will be printed to stdout. (default is: `false`)
* **-callback_debug command** - specifies a callback for debug messages when **-verbose** is `true`. The callback should accept 3 arguments: event type, raw data for particular event, response handle (can be empty for simple requests).

### Asynchronous requests

By default, requests are synchronous. If switch `-async` is specified in request parameter, an asynchronous request will be created.

Asynchronous request at the creation stage only schedule the execution of the request, but does not start any actions. Thus it never returns an error. To run an asynchronous request(s), the Tcl interpreter must enter an event loop, for example, using the `wvait` or `update` commands.

When an asynchronous request is completed with either a success or an error and a script is specified using the **-callback** option, then the script will be run. It must accept a single argument, which is the response handle. The exact state of the request and response data can be retrieved using this handle.

### Response handle

The response handle can be used to retrieve the request state, status, and other data associated with the request sent and the response received.

The following commands exist for each response handle:

* **$handle state** - returns a string corresponding to the current request state. There are the following states: `created`, `progress`, `done`, `error`.
* **$handle error** - returns an error message if an error occurs.
* **$handle status_code** - returns numeric HTTP status code (e.g. `200` or `404`).
* **$handle headers** - returns a list of headers in HTTP response.
* **$handle header header_name** - returns a list of values for particular header in HTTP response. This function is useful because HTTP headers are case-insensitive. This will avoid parsing all the headers returned by the **$handle headers** function and compare each key in a case-insensitive manner.
* **$handle content** - returns HTTP response body as is
* **$handle encoding ?encoding?** - returns or sets the encoding for HTTP body. By default, trequests attempts to automatically detect the encoding by analyzing the HTTP response header `Content-Type:`.
* **$handle text** - returns HTTP response body decoded using the response encoding
* **$handle destroy** - destroys the request handle and frees all asociated memory structures

### Sessions

Sessions is a group of requests which can share request options, TCP connections and cookies.

For example, it is possible to create a session with specific authentication parameters and/or specific HTTP request headers, and then all requests created within that session will have those default parameters.

Or it is possible to send the first request within a session with authentication credentials and expect the web server to return session cookies. Thereafter, all requests within a given session will use these session cookies.

The session handle can be created using the **::trequests::session** command, which has the following format:

* **::trequests::session ?options?** - returns a session handle that can be used to create requests within the session

The following options are accepted by **::trequests::session**:

* **-headers headers**
* **-accept value**
* **-content_type value**
* **-allow_redirects boolean**
* **-timeout milliseconds**
* **-timeout_connect milliseconds**
* **-verify_host boolean**
* **-verify_peer boolean**
* **-verify boolean**
* **-verify_status boolean**
* **-auth username_password**
* **-auth_token token**
* **-auth_scheme scheme**
* **-auth_aws_sigv4 list**
* **-verbose boolean**
* **-callback_debug command**
* **-callback command**

All these parameters mean the default settings that will be applied to requests created within this session.

A new requests within a session can be created using a session handle returned by the **::trequests::session** command:

* **$handle head url ?options?** - creates HEAD request
* **$handle get url ?options?** - creates GET request
* **$handle post url ?options?** - creates POST request
* **$handle put url ?options?** - creates PUT request
* **$handle patch url ?option?** - creates PATCH request
* **$handle delete url ?options?** - creates DELETE request
* **$handle request method url ?options?** - creates a custom request using the specified HTTP method

When a session is no longer needed, it should be destroyed:

* **$handle destroy** - destroys the session handle, all request belong to this session and frees all asociated memory structures


