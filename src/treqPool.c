/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "treqPool.h"
#include "treqRequest.h"

typedef struct treq_PoolDeactivateEvent {
    Tcl_Event header;
    treq_PoolType *pool;
} treq_PoolDeactivateEvent;

struct treq_PoolType {

    CURLM *curl_multi;

    treq_LinkedListType *requests;

    int is_dead;
    int is_active;
    int need_refresh;
    int active_connection_count;

};

typedef struct ThreadSpecificData {

    treq_PoolType *pool_default;

} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

static Tcl_EventSetupProc treq_PoolEventSetup;
static Tcl_EventCheckProc treq_PoolEventCheck;
static Tcl_EventProc treq_PoolDeactivateEventProc;
static void treq_PoolActivate(treq_PoolType *pool);
static void treq_PoolDeactivate(treq_PoolType *pool);

static void treq_PoolEventSetup(ClientData clientData, int flags) {

    // Ignore non-file events
    if (!(flags & TCL_FILE_EVENTS)) {
        return;
    }

    treq_PoolType *pool = (treq_PoolType *)clientData;

    DBG2(printf("enter..."));

    // This procedure sets the time that the Tcl core is allowed to block
    // the process from waiting for internal events before it starts
    // checking other event sources.
    //
    // If we just added a new curl handle, we need to update the state
    // as soon as possible. Therefore, we will set the wait time to 0.
    //
    // If we are serving some transfers, instruct the Tcl core not to block
    // more than 100 milliseconds before checking the curl event source.
    //
    // Also, we ask curl if it has any timeouts that need to be handled soon.
    // If anything, instruct the Tcl core not to wait longer than the required
    // time before checking the curl event source.

    Tcl_Time time = { 0, 0 };

    if (pool->need_refresh) {

        // Do not set the timeout as it is set to the default value of 0.
        DBG2(printf("need to update the state as soon as possible"));

        goto done;

    }

    // Ask curl if it has internal timeout values
    long curl_timeout;
    if (curl_multi_timeout(pool->curl_multi, &curl_timeout) == CURLM_OK) {

        if (curl_timeout < 0) {
            DBG2(printf("curl has no internal timeouts"));
        } else if (curl_timeout < 2) {
            DBG2(printf("curl wants to update as soon as possible (in %ld millisecond(s))", curl_timeout));
            pool->need_refresh = 1;
            goto done;
        } else if (curl_timeout < 100) {
            DBG2(printf("curl wants to update in %ld millisecond(s)", curl_timeout));
            time.usec = curl_timeout * 1000;
            goto done;
        } else {
            DBG2(printf("curl wants to update in %ld millisecond(s), but this value is too high", curl_timeout));
        }

    } else {
        DBG2(printf("ERROR: curl_multi_timeout failed"));
    }

    DBG2(printf("set Tcl_SetMaxBlockTime() to 100 milliseconds"));
    time.usec = 100 * 1000;

done:

    Tcl_SetMaxBlockTime(&time);

    DBG2(printf("return: ok"));

}

static void treq_PoolEventCheck(ClientData clientData, int flags) {

    // Ignore non-file events
    if (!(flags & TCL_FILE_EVENTS)) {
        return;
    }

    treq_PoolType *pool = (treq_PoolType *)clientData;

    DBG2(printf("enter..."));

    if (pool->need_refresh) {
        pool->need_refresh = 0;
        DBG2(printf("need to update the state as soon as possible"));
        goto perform;
    }

    // Actually, we can do curl_multi_perform() right here. In treq_PoolEventSetup(),
    // we allowed the Tcl core to wait 100ms when waiting for internal events.
    // However, it is possible that there are other event sources that instruct
    // Tcl core to process event sources as fast as possible (with zero wait time).
    // All event sources are handled in a fair discipline, so this event checker
    // will also run without delay. This may cause busy-loop and significant CPU
    // usage unnecessarily.
    //
    // To avoid this case, we will do poll, but with a minimum wait time
    // of 1 millisecond. This should not block significantly block other
    // Tcl events, but it should be enough to avoid a busy loop.

    int numfds;
    if (curl_multi_wait(pool->curl_multi, NULL, 0, 1, &numfds) != CURLM_OK) {
        DBG2(printf("ERROR: curl_multi_poll failed"));
        return;
    }

    if (numfds == 0) {
        DBG2(printf("return: ok (no new events)"));
        return;
    }

    DBG2(printf("curl has reported about %d waiting file descriptor(s)", numfds));

perform:

    if (curl_multi_perform(pool->curl_multi, &numfds) != CURLM_OK) {
        DBG2(printf("ERROR: curl_multi_perform failed"));
        return;
    }

    if (numfds == pool->active_connection_count) {
        DBG2(printf("we have %d active connection(s) and all of them are in progress", numfds));
        return;
    }

    DBG2(printf("we have %d active connection(s), but curl reported abut %d active connection(s)",
        pool->active_connection_count, numfds));

    CURLMsg *msg;
    int msgs_left;

    while((msg = curl_multi_info_read(pool->curl_multi, &msgs_left)) != NULL) {

        // Ignore any message that is not CURLMSG_DONE
        if (msg->msg != CURLMSG_DONE) {
            DBG2(printf("got unknown message, ignore it"));
            continue;
        }

        treq_RequestType *request;
        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &request);

        DBG2(printf("request %p completed with %s", (void *)request,
            (msg->data.result == CURLE_OK ? "OK" : "ERROR")));

        request->state = (msg->data.result == CURLE_OK ? TREQ_REQUEST_DONE : TREQ_REQUEST_ERROR);

        treq_PoolRemoveRequest(request);
        treq_RequestScheduleCallback(request);

    }

    if (numfds == 0) {
        DBG2(printf("no more active transfers, queue an event to deactivate curl event source"));
        // We want to remove the curl event source. But we can not remove it
        // right now. We are currently in the Tcl_DoOneEvent() procedure,
        // which iterates through event sources list. Thus, we can not change
        // this event sources list.
        treq_PoolDeactivateEvent *event = ckalloc(sizeof(treq_PoolDeactivateEvent));
        event->header.proc = treq_PoolDeactivateEventProc;
        event->pool = pool;
        Tcl_QueueEvent((Tcl_Event *)event, TCL_QUEUE_HEAD);
    }

    DBG2(printf("return: ok"));

}

static int treq_PoolDeactivateEventProc(Tcl_Event *evPtr, int flags) {
    UNUSED(flags);
    treq_PoolType *pool = ((treq_PoolDeactivateEvent *)evPtr)->pool;
    DBG2(printf("enter; pool: %p", (void *)pool));
    treq_PoolDeactivate(pool);
    DBG2(printf("return: ok"));
    return 1;
}

static void treq_PoolActivate(treq_PoolType *pool) {
    DBG2(printf("enter; pool: %p", (void *)pool));
    pool->is_active = 1;
    Tcl_CreateEventSource(treq_PoolEventSetup, treq_PoolEventCheck, (ClientData *)pool);
    DBG2(printf("return: ok"));
}

static void treq_PoolDeactivate(treq_PoolType *pool) {
    DBG2(printf("enter; pool: %p", (void *)pool));
    pool->is_active = 0;
    Tcl_DeleteEventSource(treq_PoolEventSetup, treq_PoolEventCheck, (ClientData *)pool);
    DBG2(printf("return: ok"));
}

static treq_PoolType *treq_PoolInit(void) {

    DBG2(printf("enter..."));

    treq_PoolType *pool = ckalloc(sizeof(treq_PoolType));
    memset(pool, 0, sizeof(treq_PoolType));

    pool->curl_multi = curl_multi_init();
    if (pool->curl_multi == NULL) {
        ckfree(pool);
        DBG2(printf("return: ERROR (could not create a pool)"));
        return NULL;
    }

    DBG2(printf("return: ok (%p)", (void *)pool));
    return pool;

}

int treq_PoolAddRequest(treq_RequestType *req) {

    DBG2(printf("enter..."));

    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    treq_PoolType *pool;

    if (tsdPtr->pool_default == NULL) {
        pool = treq_PoolInit();
        if (pool == NULL) {
            DBG2(printf("return: ERROR (failed to create a pool)"));
            return TCL_ERROR;
        }
        tsdPtr->pool_default = pool;
    } else {
        pool = tsdPtr->pool_default;
    }

    DBG2(printf("add the request to the pool"));
    if (curl_multi_add_handle(pool->curl_multi, req->curl_easy) != CURLM_OK) {
        DBG2(printf("return: ERROR (failed to add the request to the pool)"));
        return TCL_ERROR;
    }

    treq_LinkedListInsertNewItem(pool->requests, req);
    req->pool = pool;

    if (!pool->is_active) {
        treq_PoolActivate(pool);
    }

    pool->active_connection_count++;
    pool->need_refresh = 1;

    DBG2(printf("return: ok"));
    return TCL_OK;

}

void treq_PoolRemoveRequest(treq_RequestType *req) {

    treq_PoolType *pool = req->pool;

    DBG2(printf("enter; pool: %p remove: %p", (void *)pool, (void *)req));

    curl_multi_remove_handle(pool->curl_multi, req->curl_easy);

    if (!pool->is_dead) {
        treq_LinkedListRemoveByItem(pool->requests, req);
    }
    req->pool = NULL;

    pool->active_connection_count--;

    if (req->state == TREQ_REQUEST_INPROGRESS) {
        DBG2(printf("set request state as ERROR"));
        req->state = TREQ_REQUEST_ERROR;
        treq_RequestSetError(req, Tcl_NewStringObj("the request has been removed from async pool", -1));
        // TODO: if there is a callback in the request, we need to make sure it is running,
        // as we need to report the above error to that callback
    }

    DBG2(printf("return: ok"));

}

static void treq_PoolFree(treq_PoolType *pool) {

    DBG2(printf("enter; pool: %p", (void *)pool));

    pool->is_dead = 1;

    DBG2(printf("cleanup pool requests"));

    treq_LinkedListFree(pool->requests) {
        DBG2(printf("cleanup request: %p", (void *)pool->requests->item));
        treq_PoolRemoveRequest((treq_RequestType *)pool->requests->item);
    }

    curl_multi_cleanup(pool->curl_multi);

    ckfree(pool);

    DBG2(printf("return: ok"));

}

void treq_PoolThreadExitProc(void) {

    DBG2(printf("enter..."));

    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (tsdPtr->pool_default != NULL) {
        DBG2(printf("release pool"));
        treq_PoolFree(tsdPtr->pool_default);
        tsdPtr->pool_default = NULL;
    }

    DBG2(printf("return: ok"));

}

