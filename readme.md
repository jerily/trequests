<!--
/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
-->

# trequests

TCL/C extension that provides an interface to the [libcurl](https://curl.se/libcurl/) C library. It contains functions for making synchronous and asynchronous HTTP/HTTPS requests, specifying parameters for GET/POST request types, working with response data and creating sessions in which requests share parameters, TCP connections, cookies.

## Requirements

- Tcl 8.6 or 9.x headers and stub library (tested with 8.6 and 9.1).
- [libcurl](https://curl.se/libcurl/) 7.87 or newer with a TLS backend for HTTPS.
- A C11 compiler and CMake 3.22.1 or newer.

Supported systems:

- Linux
- MacOS

## Installation

### Install Dependencies

Make sure that the build environment has [cURL development packages](https://curl.se/download.html) or build cURL from [sources](https://github.com/curl/curl). For example, on Ubuntu Linux, the development package can be installed using the following commands:

```bash
sudo apt update
sudo apt install -y libcurl4-openssl-dev
```

### Install trequests

```bash
git clone https://github.com/jerily/trequests.git
cd trequests
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build
```

For a custom Tcl installation, set `CMAKE_PREFIX_PATH`, or explicitly set
`TCL_INCLUDE_PATH`, `TCL_STUB_LIBRARY` and `TCL_TCLSH` to matching files.
Shared builds use Tcl stubs and do not link directly to libtcl. Build separately
for Tcl 8.6 and Tcl 9; stubs do not make their major-version ABIs interchangeable.
`-DBUILD_SHARED_LIBS=OFF` produces a static archive for embedding: link the host
with Tcl and libcurl, call `Trequests_Init`, and register it with `Tcl_StaticPackage`.

Run the offline options and loopback HTTP tests with:

```bash
cmake -S . -B build -DTREQUESTS_TESTING_MODE=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`TREQUESTS_TESTING_MODE` exposes diagnostic commands and is off by default.
`TREQUESTS_NETWORK_TESTS=ON` also enables the existing external-service tests.
Use `TCL_MEM_DEBUG=ON` only with a matching memory-debug Tcl build.

## TLS and binary data

Certificate and hostname verification remain enabled by default. Use `-cafile`
to trust a PEM CA bundle, including a private CA, without disabling verification.
Sessions inherit this setting; an individual request can override it.
Transport errors are available through `$response state` and `$response error`;
HTTP errors still have their HTTP status code.

```tcl
package require trequests 1.1.0
set session [::trequests::session -cafile /path/to/ca.pem]
set response [$session put https://example.com/object \
    -data_binary [binary format H* 0080ff] -content_type application/octet-stream]
try {
    if {[$response state] eq "error"} {error [$response error]}
    puts [$response status_code]
} finally {$session destroy}
```

`-data_binary` sends a Tcl byte array unchanged, including embedded NULs and all
byte values. `$response content` returns a byte array; use `$response text` for
decoded text. This option is exclusive with the text, JSON and form body options.
It works for synchronous and asynchronous requests. When sending textual data
as bytes, explicitly encode it first, for example `encoding convertto utf-8`.

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
* **-cafile path** - PEM CA bundle used to verify the peer; defaults to libcurl's configured trust store.

#### Authentication options

* **-auth username_password** - specifies a list of 2 elements **username** and **password**. These credentials will be used for Basic Authentication.
* **-auth_token token** - specifies a token which will be used in Bearer Authentication.
* **-auth_scheme scheme** - specifies the authentication schemes that can be used. Accepts a list with 1 or more elements of the following authentication scheme names: `basic`, `digest`, `digest_ie`, `bearer`, `negotiate`, `ntlm`, `aws_sigv4`. It can also take a single value of `any` or `anysafe` that corresponds to any authentication scheme or authentication schemes that are considered secure.
* **-auth_aws_sigv4 list** - specifies the parameters for AWS V4 signature authentication. Accepts a number of list elements from 1 to 4, which will be concatenated with a colon and passed to the cURL parameter [CURLOPT_AWS_SIGV4](https://curl.se/libcurl/c/CURLOPT_AWS_SIGV4.html).

#### URL query parameters

* **-params data** - takes an even list of elements, which are treated as key-value pairs. Each key and value will be urlencoded and concatenated with an equals symbol (`=`). All key-value pairs will be concatenated with an ampersand character (`&`). Can be specified multiple times.
* **-params_raw data** - accepts a string that will be used in the URL request parameters as is. Can be specified multiple times. All strings specified by this parameter will be concatenated with an ampersand character (`&`).

#### POST parameters

* **-data_binary bytes** - sends one byte array without encoding or concatenation; mutually exclusive with other body options.
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

Asynchronous requests schedule execution after validating their arguments. To run them, enter the Tcl event loop with `vwait` or `update`. Transport errors are reported on the response handle.

When an asynchronous request is completed with either a success or an error and a script is specified using the **-callback** option, then the script will be run. It must accept a single argument, which is the response handle. The exact state of the request and response data can be retrieved using this handle.

### Streaming response bodies (1.2.0)

`-callback_data commandPrefix` delivers each body chunk as a Tcl byte array,
followed by the response handle. It requires `-async`; the existing `-callback`
still runs once at completion. Data callbacks run through Tcl's event queue,
outside libcurl, in arrival order before completion. They can cancel or destroy
the request (or its session). An error from a data callback stops the request
and becomes its `$handle error`. Remaining data callbacks are suppressed.

`-buffer_response false` disables body retention and requires `-callback_data`.
The default is true, preserving existing `$handle content`/`text` behavior.
Neither option is inherited from sessions; set it on each request.

```tcl
package require trequests 1.2
proc received {bytes response} {
    # bytes may end inside a UTF-8 character or application-level message.
    puts "Received [string length $bytes] bytes"
}
proc completed {response} {set ::done 1}
set done 0
set response [::trequests::get https://example.com/stream -async \
    -callback_data received -buffer_response false -callback completed]
try {
    vwait done
    if {[$response state] eq "error"} {error [$response error]}
    puts [$response status_code]
} finally {$response destroy}
```

`$handle cancel` aborts the transfer and schedules completion once with state
`error` and message `request cancelled`. Destroying a handle suppresses its
remaining callbacks. Keep callbacks short and avoid nested event loops. Pending
body callbacks are capped at 1 MiB per request; overflow aborts explicitly rather
than losing data or allocating an unbounded queue. Applications must frame and
decode bytes themselves; the HTTP client does not assume SSE or JSON.

Offline `stream.test` uses a gated local server: the server cannot finish until
the body callback releases it. It also exercises binary data, buffering,
callback errors, cancellation, and request/session destruction during callbacks.
Build and test separately against Tcl 8.6 and Tcl 9. Static builds continue to
provide `Trequests_Init`; no OpenAI-specific behavior is part of this extension.
Release validation: 198 offline tests pass on both Tcl 8.6 and 9.1 (one
feature-dependent test is skipped); the same suite passes under
AddressSanitizer/UBSan with leak detection disabled. Static embedding is also
validated through the OpenAI Tcl client suite.
The root `ttrek.json` contains the 1.2.0 package recipe.

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
* **$handle cancel** - cancels an outstanding request and schedules its completion callback with an error.
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
* **-cafile path**
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


