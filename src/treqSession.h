/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TREQUESTS_TREQSESSION_H
#define TREQUESTS_TREQSESSION_H

#include "common.h"

typedef struct treq_SessionRequestsListType treq_SessionRequestsListType;

struct treq_SessionType {
    Tcl_Interp *interp;
    Tcl_Command cmd_token;
    Tcl_Obj *cmd_name;
    Tcl_Obj *trace_var;

    CURLSH *curl_share;

    Tcl_Obj *headers;
    treq_RequestAuthType *auth;
    int allow_redirects;
    int verbose;
    Tcl_Obj *callback;
    Tcl_Obj *callback_debug;
    Tcl_Obj *accept;
    Tcl_Obj *content_type;
    int timeout;
    int timeout_connect;
    int verify;
    int verify_host;
    int verify_peer;
    int verify_status;

    treq_LinkedListType *requests;
};

#ifdef __cplusplus
extern "C" {
#endif

treq_SessionType *treq_SessionInit(void);
treq_RequestType *treq_SessionRequestInit(treq_SessionType *ses);
void treq_SessionRemoveRequest(treq_RequestType *req);
void treq_SessionFree(treq_SessionType *ses);

void treq_SessionThreadExitProc(void);

#ifdef __cplusplus
}
#endif

#endif // TREQUESTS_TREQSESSION_H
