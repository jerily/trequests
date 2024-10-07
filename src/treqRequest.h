/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TREQUESTS_TREQREQUEST_H
#define TREQUESTS_TREQREQUEST_H

#include "common.h"

// Values of this enum must start at 1 to differentiate between NULL
// and a real value
typedef enum {
    TREQ_METHOD_HEAD = 1,
    TREQ_METHOD_GET,
    TREQ_METHOD_POST,
    TREQ_METHOD_PUT,
    TREQ_METHOD_PATCH,
    TREQ_METHOD_DELETE,
    TREQ_METHOD_CUSTOM
} treq_RequestMethodType;

typedef enum {
    TREQ_REQUEST_CREATED,
    TREQ_REQUEST_INPROGRESS,
    TREQ_REQUEST_DONE,
    TREQ_REQUEST_ERROR
} treq_RequestStateType;

typedef struct treq_RequestEvent treq_RequestEvent;

struct treq_RequestType {

    Tcl_Interp *interp;
    Tcl_Command cmd_token;
    Tcl_Obj *cmd_name;
    Tcl_Obj *trace_var;
    treq_PoolType *pool;

    CURL *curl_easy;
    CURLU *curl_url;

    char curl_error[CURL_ERROR_SIZE];
    Tcl_Obj *error;
    treq_RequestStateType state;

    treq_SessionType *session;
    int isDead;

    // Input parameters

    Tcl_Obj *url;
    Tcl_Obj *headers;
    struct curl_slist *curl_headers;
    Tcl_Obj *header_accept;
    Tcl_Obj *header_content_type;

    Tcl_Obj *form;
    curl_mime *curl_mime;

    treq_RequestMethodType method;
    Tcl_Obj *custom_method;

    treq_RequestAuthType *auth;

    Tcl_Obj *postfields;
    Tcl_Obj *querystring;

    int allow_redirects;
    int verbose;
    int timeout;
    int timeout_connect;

    int verify_host;
    int verify_peer;
    int verify_status;

    Tcl_Obj *callback;
    treq_RequestEvent *callback_event;
    int async;

    Tcl_Obj *callback_debug;

    // Output parameters

    // We don't use Tcl_Obj or DString to store the content, as we don't want
    // to crash on a memory allocation error. A memory allocation error is
    // very likely when we download something unknown from network.
    char *content;
    Tcl_Size content_size;

    Tcl_Encoding encoding;
    Tcl_Obj *content_type;
    Tcl_Obj *content_charset;

    // Other

#ifdef TREQUESTS_TESTING_MODE
    Tcl_Obj *set_options;
#endif

};

typedef Tcl_Obj *(treq_RequestGetterProc)(treq_RequestType *req);

#ifdef __cplusplus
extern "C" {
#endif

treq_RequestType *treq_RequestInit(void);
void treq_RequestFree(treq_RequestType *req);
void treq_RequestRun(treq_RequestType *req);

treq_RequestGetterProc treq_RequestGetError;
treq_RequestGetterProc treq_RequestGetContent;
treq_RequestGetterProc treq_RequestGetText;
treq_RequestGetterProc treq_RequestGetHeaders;
treq_RequestGetterProc treq_RequestGetEncoding;
treq_RequestGetterProc treq_RequestGetStatusCode;
treq_RequestGetterProc treq_RequestGetState;

Tcl_Obj *treq_RequestGetHeader(treq_RequestType *req, const char *header);

void treq_RequestSetError(treq_RequestType *req, Tcl_Obj *error);
void treq_RequestSetEncoding(treq_RequestType *req, Tcl_Encoding encoding);

void treq_RequestScheduleCallback(treq_RequestType *req);

const char *treq_RequestGetMethodName(treq_RequestMethodType method);

#ifdef __cplusplus
}
#endif

#endif // TREQUESTS_TREQREQUEST_H
