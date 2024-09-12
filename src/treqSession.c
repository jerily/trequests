/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "treqSession.h"
#include "treqRequest.h"

typedef struct ThreadSpecificData {

    Tcl_Obj *obj;

} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

treq_SessionType *treq_SessionInit(void) {

    DBG2(printf("enter..."));

    treq_SessionType *ses = ckalloc(sizeof(treq_SessionType));
    memset(ses, 0, sizeof(treq_SessionType));

    ses->curl_share = curl_share_init();
    if (ses->curl_share == NULL) {
        goto error;
    }

    curl_share_setopt(ses->curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
    curl_share_setopt(ses->curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(ses->curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(ses->curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
    curl_share_setopt(ses->curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_PSL);
    curl_share_setopt(ses->curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_HSTS);

    DBG2(printf("return: %p", (void *)ses));
    return ses;

error:
    treq_SessionFree(ses);
    DBG2(printf("return: ERROR (failed to alloc)"));
    return NULL;

}

treq_RequestType *treq_SessionRequestInit(treq_SessionType *ses) {

    DBG2(printf("enter..."));

    treq_RequestType *req = treq_RequestInit();
    if (req == NULL) {
        return NULL;
    }

    if (ses->headers != NULL) {
        req->headers = ses->headers;
        Tcl_IncrRefCount(req->headers);
    }

    if (ses->allow_redirects != -1) {
        req->allow_redirects = ses->allow_redirects;
    }

    if (ses->verbose != -1) {
        req->verbose = ses->verbose;
    }

    req->session = ses;
    curl_easy_setopt(req->curl_easy, CURLOPT_SHARE, ses->curl_share);
    // Turn on cookie parser
    curl_easy_setopt(req->curl_easy, CURLOPT_COOKIEFILE, "");

    treq_LinkedListInsertNewItem(ses->requests, req);

    DBG2(printf("return: %p", (void *)req));
    return req;

}

void treq_SessionRemoveRequest(treq_RequestType *req) {

    treq_SessionType *ses = req->session;

    DBG2(printf("enter; ses: %p remove: %p", (void *)ses, (void *)req));

    req->session = NULL;
    treq_LinkedListRemoveByItem(ses->requests, req);

    DBG2(printf("return: ok"));

}

void treq_SessionFree(treq_SessionType *ses) {

    DBG2(printf("enter; ses: %p", (void *)ses));

    DBG2(printf("cleanup session requests"));

    treq_LinkedListFree(ses->requests) {
        DBG2(printf("cleanup request: %p", (void *)ses->requests->item));
        ((treq_RequestType *)ses->requests->item)->session = NULL;
        treq_RequestFree((treq_RequestType *)ses->requests->item);
    }

    if (ses->curl_share != NULL) {
        curl_share_cleanup(ses->curl_share);
    }

    if (ses->headers != NULL) {
        Tcl_DecrRefCount(ses->headers);
    }

    ckfree(ses);

    DBG2(printf("return: ok"));
    return;

}

void treq_SessionThreadExitProc(void) {
    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    DBG2(printf("enter..."));
    UNUSED(tsdPtr);
    DBG2(printf("return: ok"));
}

