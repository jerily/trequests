/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TREQUESTS_TREQREQUEST_H
#define TREQUESTS_TREQREQUEST_H

#include "common.h"

typedef enum {
    TREQ_METHOD_HEAD,
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

struct treq_RequestType {

    Tcl_Interp *interp;
    Tcl_Command cmd_token;
    treq_PoolType *pool;

    CURL *curl_easy;

    char curl_error[CURL_ERROR_SIZE];
    Tcl_Obj *error;
    treq_RequestStateType state;

    treq_SessionType *session;
    int isDead;

    // Input parameters

    Tcl_Obj *url;
    Tcl_Obj *headers;
    struct curl_slist *curl_headers;

    Tcl_Obj *data_form;
    curl_mime *curl_mime;

    treq_RequestMethodType method;
    Tcl_Obj *custom_method;

    int allow_redirects;
    int verbose;

    // Output parameters

    // We don't use Tcl_Obj or DString to store the content, as we don't want
    // to crash on a memory allocation error. A memory allocation error is
    // very likely when we download something unknown from network.
    char *content;
    Tcl_Size content_size;

    Tcl_Encoding encoding;
    Tcl_Obj *content_type;
    Tcl_Obj *content_charset;

};

#ifdef __cplusplus
extern "C" {
#endif

treq_RequestType *treq_RequestInit(void);
void treq_RequestFree(treq_RequestType *req);
void treq_RequestRun(treq_RequestType *req);

Tcl_Obj *treq_RequestGetError(treq_RequestType *req);
Tcl_Obj *treq_RequestGetContent(treq_RequestType *req);
Tcl_Obj *treq_RequestGetText(treq_RequestType *req);
Tcl_Obj *treq_RequestGetHeaders(treq_RequestType *req);
Tcl_Obj *treq_RequestGetHeader(treq_RequestType *req, const char *header);
Tcl_Obj *treq_RequestGetEncoding(treq_RequestType *req);
void treq_RequestSetEncoding(treq_RequestType *req, Tcl_Encoding encoding);
Tcl_Obj *treq_RequestGetStatusCode(treq_RequestType *req);

#ifdef __cplusplus
}
#endif

#endif // TREQUESTS_TREQREQUEST_H
