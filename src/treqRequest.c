/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "treqRequest.h"
#include "treqSession.h"
#include "treqPool.h"
#include "treqRequestAuth.h"

typedef struct treq_RequestEvent {
    Tcl_Event header;
    treq_RequestType *request;
} treq_RequestEvent;

static Tcl_EventProc treq_RequestEventProc;

static int treq_RequestEventProc(Tcl_Event *evPtr, int flags) {

    // Ignore non-file events
    if (!(flags & TCL_FILE_EVENTS)) {
        return 0;
    }

    DBG2(printf("enter"));

    treq_RequestType *req = ((treq_RequestEvent *)evPtr)->request;

    if (req == NULL) {
        DBG2(printf("return: ok (request has already been destroyed)"));
        return 1;
    }

    req->callback_event = NULL;

    treq_ExecuteTclCallback(req->interp, req->callback, 1, &req->cmd_name, 1);

    DBG2(printf("return: ok"));
    return 1;

}

void treq_RequestScheduleCallback(treq_RequestType *req) {

    DBG2(printf("enter"));

    if (req->callback == NULL) {
        DBG2(printf("return: ok (request has no callback)"));
        return;
    }

    if (Tcl_InterpDeleted(req->interp)) {
        DBG2(printf("return: ok (interp is deleted)"));
        return;
    }

    treq_RequestEvent *event = ckalloc(sizeof(treq_RequestEvent));
    event->header.proc = treq_RequestEventProc;
    event->request = req;
    req->callback_event = event;
    Tcl_QueueEvent((Tcl_Event *)event, TCL_QUEUE_TAIL);

    DBG2(printf("return: ok"));

}

static int treq_RequestUpdateContentType(treq_RequestType *req) {
    DBG2(printf("enter"));

    struct curl_header *h;
    if (curl_easy_header(req->curl_easy, "Content-Type", 0, CURLH_HEADER, -1, &h) != CURLHE_OK) {
        DBG2(printf("return: ok (server didn't set Content-Type header yet)"));
        return 0;
    }

    DBG2(printf("server content type: [%s]", h->value));

    treq_ParseContentType(h->value, &req->content_type, &req->content_charset);
    Tcl_IncrRefCount(req->content_type);
    Tcl_IncrRefCount(req->content_charset);

    DBG2(printf("return: ok"));
    return 1;
}

static int treq_RequestUpdateEncoding(treq_RequestType *req) {
    DBG2(printf("enter"));

    if (req->content_charset == NULL && !treq_RequestUpdateContentType(req)) {
        DBG2(printf("return: NULL (charset is unknown yet)"));
        return 0;
    }

    if (Tcl_GetCharLength(req->content_charset) == 0) {
        DBG2(printf("server didn't set charset"));
        goto useDefault;
    }

    Tcl_Obj *encoding = treq_ConvertCharsetToEncoding(req->content_charset);

    DBG2(printf("try to use encoding [%s]", Tcl_GetString(encoding)));
    req->encoding = Tcl_GetEncoding(NULL, Tcl_GetString(encoding));
    Tcl_BounceRefCount(encoding);

    if (req->encoding == NULL) {
useDefault:
        DBG2(printf("failed to use the encoding, fall-back to iso8859-1"));
        req->encoding = Tcl_GetEncoding(NULL, "iso8859-1");
        DBG2(printf("set: iso8859-1 (%p)", (void *)req->encoding));
    }

    DBG2(printf("return: ok"));
    return 1;
}

Tcl_Obj *treq_RequestGetStatusCode(treq_RequestType *req) {
    long code = 0;
    curl_easy_getinfo(req->curl_easy, CURLINFO_RESPONSE_CODE, &code);
    return Tcl_NewWideIntObj(code);
}

Tcl_Obj *treq_RequestGetEncoding(treq_RequestType *req) {
    if (req->encoding == NULL && !treq_RequestUpdateEncoding(req)) {
        DBG2(printf("no encoding set, return the default: iso8859-1"));
        return Tcl_NewStringObj("iso8859-1", -1);
    }
    DBG2(printf("get encoding %p", (void *)req->encoding));
    return Tcl_NewStringObj(Tcl_GetEncodingName(req->encoding), -1);
}

void treq_RequestSetEncoding(treq_RequestType *req, Tcl_Encoding encoding) {
    DBG2(printf("set encoding %p", (void *)encoding));
    req->encoding = encoding;
}

Tcl_Obj *treq_RequestGetError(treq_RequestType *req) {

    if (req->error != NULL) {
        if (strlen(req->curl_error) == 0) {
            return req->error;
        }
        Tcl_Obj *error = Tcl_DuplicateObj(req->error);
        Tcl_AppendPrintfToObj(error, "(cURL error: %s)", req->curl_error);
        return error;
    }

    if (strlen(req->curl_error) == 0) {
        if (req->state == TREQ_REQUEST_ERROR) {
            return Tcl_NewStringObj("unknown error", -1);
        }
        return Tcl_NewObj();
    }

    return Tcl_ObjPrintf("cURL error: %s", req->curl_error);

}

Tcl_Obj *treq_RequestGetText(treq_RequestType *req) {

    DBG2(printf("enter"));

    if (req->content == NULL) {
        return Tcl_NewObj();
    }

    Tcl_Encoding encoding;
    if (req->encoding == NULL && !treq_RequestUpdateEncoding(req)) {
        encoding = Tcl_GetEncoding(NULL, "iso8859-1");
        DBG2(printf("use default encoding"));
    } else {
        DBG2(printf("use encoding from request"));
        encoding = req->encoding;
    }

    Tcl_DString ds;
    const char *value = Tcl_ExternalToUtfDString(encoding, req->content, req->content_size, &ds);
    Tcl_Obj *result = Tcl_NewStringObj(value, Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);

    DBG2(printf("return: ok"));
    return result;

}

Tcl_Obj *treq_RequestGetContent(treq_RequestType *req) {
    return (req->content == NULL ?
        Tcl_NewObj() :
        Tcl_NewByteArrayObj((const unsigned char *)req->content, req->content_size));
}

Tcl_Obj *treq_RequestGetHeaders(treq_RequestType *req) {

    Tcl_Obj *result = Tcl_NewListObj(0, NULL);

    struct curl_header *prev = NULL;
    struct curl_header *h;

    while((h = curl_easy_nextheader(req->curl_easy, CURLH_HEADER, 0, prev))) {
        Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(h->name, -1));
        Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(h->value, -1));
        prev = h;
    }

    return result;

}

Tcl_Obj *treq_RequestGetHeader(treq_RequestType *req, const char *header) {

    struct curl_header *h;

    if (curl_easy_header(req->curl_easy, header, 0, CURLH_HEADER, -1, &h) != CURLHE_OK) {
        return NULL;
    }

    DBG2(printf("add header #0:"));
    DBG2(printf("  header name: [%s]", h->name));
    DBG2(printf("  header value: [%s]", h->value));
    DBG2(printf("  amount: [%zu]", h->amount));
    DBG2(printf("  index: [%zu]", h->index));

    Tcl_Obj *result = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(h->value, -1));

    size_t amount = h->amount;
    for (size_t i = 1; i < amount; i++) {
        if (curl_easy_header(req->curl_easy, header, i, CURLH_HEADER, -1, &h) != CURLHE_OK) {
            DBG2(printf("ERROR: failed to get header #%zu", i));
            continue;
        }
        DBG2(printf("add header #%zu", i));
        Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(h->value, -1));
    }

    return result;

}

void treq_RequestSetError(treq_RequestType *req, Tcl_Obj *error) {

    if (req->error != NULL) {
        Tcl_DecrRefCount(req->error);
    }

    DBG2(printf("set request ERROR: [%s]", Tcl_GetString(error)));
    req->error = error;
    Tcl_IncrRefCount(req->error);
    req->state = TREQ_REQUEST_ERROR;

}

Tcl_Obj *treq_RequestGetState(treq_RequestType *req) {

    const char *state;

    switch (req->state) {
    case TREQ_REQUEST_CREATED:
        state = "created";
        break;
    case TREQ_REQUEST_INPROGRESS:
        state = "progress";
        break;
    case TREQ_REQUEST_DONE:
        state = "done";
        break;
    case TREQ_REQUEST_ERROR:
        state = "error";
        break;
    }

    return Tcl_NewStringObj(state, -1);

}

// treq_debug_dump & treq_debug_callback are from curl examples

static void treq_debug_dump(const char *text, FILE *stream, unsigned char *ptr, size_t size) {
    size_t i;
    size_t c;
    unsigned int width = 0x10;

    fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n", text, (long)size, (long)size);

    for(i = 0; i < size; i += width) {
        fprintf(stream, "%4.4lx: ", (long)i);

        /* show hex to the left */
        for(c = 0; c < width; c++) {
            if(i + c < size)
                fprintf(stream, "%02x ", ptr[i + c]);
            else
                fputs("   ", stream);
        }

        /* show data on the right */
        for(c = 0; (c < width) && (i + c < size); c++) {
            char x = (ptr[i + c] >= 0x20 && ptr[i + c] < 0x80) ? ptr[i + c] : '.';
            fputc(x, stream);
        }

        fputc('\n', stream); /* newline */
    }
}

static int treq_debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *clientp) {
    const char *text;
    UNUSED(handle);
    treq_RequestType *req = (treq_RequestType *)clientp;

    if (req->callback_debug == NULL) {
        goto internalDebugOutput;
    }

    switch(type) {
    case CURLINFO_TEXT:
        text = "info";
        break;
    case CURLINFO_HEADER_OUT:
        text = "header_out";
        break;
    case CURLINFO_HEADER_IN:
        text = "header_in";
        break;
    case CURLINFO_DATA_OUT:
        text = "data_out";
        break;
    case CURLINFO_DATA_IN:
        text = "data_in";
        break;
    case CURLINFO_SSL_DATA_OUT:
        text = "ssl_data_out";
        break;
    case CURLINFO_SSL_DATA_IN:
        text = "ssl_data_in";
        break;
    case CURLINFO_END:
        return 0;
    }

    Tcl_Obj *objv[3] = {
        Tcl_NewStringObj(text, -1),
        (type == CURLINFO_TEXT ? Tcl_NewStringObj(data, size) : Tcl_NewByteArrayObj((const unsigned char *)data, size)),
        (req->cmd_name == NULL ? Tcl_NewObj() : req->cmd_name)
    };

    treq_ExecuteTclCallback(req->interp, req->callback_debug, 3, objv, 0);

    goto done;

internalDebugOutput:

    switch(type) {
    case CURLINFO_TEXT:
        fputs("== Info: ", stderr);
        fwrite(data, size, 1, stderr);
        return 0;
    case CURLINFO_HEADER_OUT:
        text = "=> Send header";
        break;
    case CURLINFO_DATA_OUT:
        text = "=> Send data";
        break;
    case CURLINFO_SSL_DATA_OUT:
        text = "=> Send SSL data";
        break;
    case CURLINFO_HEADER_IN:
        text = "<= Recv header";
        break;
    case CURLINFO_DATA_IN:
        text = "<= Recv data";
        break;
    case CURLINFO_SSL_DATA_IN:
        text = "<= Recv SSL data";
        break;
    case CURLINFO_END:
        return 0;
    }

    treq_debug_dump(text, stderr, (unsigned char *)data, size);

done:
    return 0;
}

static size_t treq_write_callback(const char *ptr, size_t size, size_t nmemb, void *userdata) {

    treq_RequestType *req = (treq_RequestType *)userdata;
    size = size * nmemb;

    DBG2(printf("enter; existing buffer size: %" TCL_SIZE_MODIFIER "d; add chunk size: %zu", req->content_size, size));

    if (size > 0) {

        if (req->content_size == 0) {
            req->content = attemptckalloc(size);
            if (req->content == NULL) {
                goto error;
            }
        } else {
            char *tmp = attemptckrealloc(req->content, req->content_size + size);
            if (tmp == NULL) {
                goto error;
            }
            req->content = tmp;
        }

        memcpy(&req->content[req->content_size], ptr, size);
        req->content_size += size;

    }

    return size;

error:

    treq_RequestSetError(req, Tcl_ObjPrintf("failed to allocate %ld additional bytes in"
        " the output buffer, current output buffer size is %ld", (long)size, (long)req->content_size));
    return CURL_WRITEFUNC_ERROR;

}

void treq_RequestRun(treq_RequestType *req) {

#define safe_curl_easy_setopt(opt,val) \
    { \
        CURLcode __curl_res = curl_easy_setopt(req->curl_easy, (opt), (val)); \
        if (__curl_res != CURLE_OK) { \
            treq_RequestSetError(req, Tcl_ObjPrintf("curl_easy_setopt(%s) failed: %s", #opt, curl_easy_strerror(__curl_res))); \
            return; \
        } \
    }

    DBG2(printf("enter..."));

    DBG2(printf("url: [%s]", Tcl_GetString(req->url)));
    safe_curl_easy_setopt(CURLOPT_URL, Tcl_GetString(req->url));

    switch (req->method) {
    case TREQ_METHOD_HEAD:
        DBG2(printf("use method %s", "HEAD"));
        safe_curl_easy_setopt(CURLOPT_HTTPGET, 1L);
        safe_curl_easy_setopt(CURLOPT_NOBODY, 1L);
        break;
    case TREQ_METHOD_GET:
        DBG2(printf("use method %s", "GET"));
        safe_curl_easy_setopt(CURLOPT_HTTPGET, 1L);
        break;
    case TREQ_METHOD_POST:
        DBG2(printf("use method %s", "POST"));
        safe_curl_easy_setopt(CURLOPT_POST, 1L);
        break;
    case TREQ_METHOD_PUT:
        DBG2(printf("use method %s", "PUT"));
        safe_curl_easy_setopt(CURLOPT_UPLOAD, 1L);
        break;
    case TREQ_METHOD_PATCH:
        DBG2(printf("use method %s", "PATCH"));
        safe_curl_easy_setopt(CURLOPT_CUSTOMREQUEST, "PATCH");
        break;
    case TREQ_METHOD_DELETE:
        DBG2(printf("use method %s", "DELETE"));
        safe_curl_easy_setopt(CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    case TREQ_METHOD_CUSTOM:
        DBG2(printf("use custom method [%s]", Tcl_GetString(req->custom_method)));
        safe_curl_easy_setopt(CURLOPT_CUSTOMREQUEST, Tcl_GetString(req->custom_method));
        break;
    }

    if (req->auth != NULL) {

        if (req->auth->username != NULL) {
            safe_curl_easy_setopt(CURLOPT_USERNAME, Tcl_GetString(req->auth->username));
        }

        if (req->auth->password != NULL) {
            safe_curl_easy_setopt(CURLOPT_PASSWORD, Tcl_GetString(req->auth->password));
        }

        if (req->auth->token != NULL) {
            safe_curl_easy_setopt(CURLOPT_XOAUTH2_BEARER, Tcl_GetString(req->auth->token));
        }

        if (req->auth->aws_sigv4 != NULL) {
            safe_curl_easy_setopt(CURLOPT_AWS_SIGV4, Tcl_GetString(req->auth->aws_sigv4));
        }

        if (req->auth->scheme != -1) {
            safe_curl_easy_setopt(CURLOPT_HTTPAUTH, req->auth->scheme);
        } else if (req->auth->token != NULL) {
            safe_curl_easy_setopt(CURLOPT_HTTPAUTH, CURLAUTH_BEARER | CURLAUTH_ONLY);
        }

    }

    if (req->form != NULL) {

        DBG2(printf("add form data..."));

        req->curl_mime = curl_mime_init(req->curl_easy);
        curl_mimepart *field = NULL;

        Tcl_DictSearch search;
        Tcl_Obj *key, *value;
        int done;

        Tcl_DictObjFirst(NULL, req->form, &search, &key, &value, &done);
        for (; !done ; Tcl_DictObjNext(&search, &key, &value, &done)) {

            field = curl_mime_addpart(req->curl_mime);

            curl_mime_name(field, Tcl_GetString(key));

            Tcl_Size value_len;
            const char *value_str = Tcl_GetStringFromObj(value, &value_len);
            curl_mime_data(field, value_str, value_len);

            DBG2(printf("add form field [%s] = [%s]", Tcl_GetString(key), value_str));

        }
        Tcl_DictObjDone(&search);

        safe_curl_easy_setopt(CURLOPT_MIMEPOST, req->curl_mime);

    }

    safe_curl_easy_setopt(CURLOPT_FOLLOWLOCATION, (req->allow_redirects ? 1L : 0L));
    safe_curl_easy_setopt(CURLOPT_VERBOSE, (req->verbose ? 1L : 0L));

    if (req->header_accept != NULL) {
        req->curl_headers = curl_slist_append(req->curl_headers, Tcl_GetString(req->header_accept));
    }

    if (req->header_content_type != NULL) {
        req->curl_headers = curl_slist_append(req->curl_headers, Tcl_GetString(req->header_content_type));
    }

    if (req->headers != NULL) {

        Tcl_DictSearch search;
        Tcl_Obj *key, *value;
        int done;

        Tcl_DictObjFirst(NULL, req->headers, &search, &key, &value, &done);
        for (; !done ; Tcl_DictObjNext(&search, &key, &value, &done)) {

            Tcl_Obj *obj = Tcl_DuplicateObj(key);
            Tcl_AppendToObj(obj, ": ", 2);
            Tcl_AppendObjToObj(obj, value);

            DBG2(printf("add header: [%s]", Tcl_GetString(obj)));
            req->curl_headers = curl_slist_append(req->curl_headers, Tcl_GetString(obj));
            Tcl_BounceRefCount(obj);

            if (req->curl_headers == NULL) {
                treq_RequestSetError(req, Tcl_NewStringObj("failed to add headers", -1));
                return;
            }

        }
        Tcl_DictObjDone(&search);

    }

    // Disable cURL misbehavior in the same cases. See:
    //     * https://curl.se/mail/lib-2017-07/0013.html
    //     * https://gms.tf/when-curl-sends-100-continue.html
    //     * https://stackoverflow.com/questions/49670008/how-to-disable-expect-100-continue-in-libcurl
    //DBG2(printf("disable Expect: 100-continue"));
    //req->curl_headers = curl_slist_append(req->curl_headers, "Expect:");

    if (req->curl_headers != NULL) {
        safe_curl_easy_setopt(CURLOPT_HTTPHEADER, req->curl_headers);
    }

    req->state = TREQ_REQUEST_INPROGRESS;

    if (req->async) {

        if (treq_PoolAddRequest(req) != TCL_OK) {
            req->state = TREQ_REQUEST_ERROR;
            treq_RequestSetError(req, Tcl_NewStringObj("failed to add the request to the pool", -1));
            treq_RequestScheduleCallback(req);
        }

    } else {

        DBG2(printf("run cURL request..."));
        CURLcode res = curl_easy_perform(req->curl_easy);

        if (res == CURLE_OK) {
            req->state = TREQ_REQUEST_DONE;
            DBG2(printf("request: ok"));
        } else {
            req->state = TREQ_REQUEST_ERROR;
            DBG2(printf("request: ERROR (%s)", Tcl_GetString(treq_RequestGetError(req))));
        }

    }

    DBG2(printf("return: ok"));
    return;

}

treq_RequestType *treq_RequestInit(void) {

    DBG2(printf("enter..."));

    treq_RequestType *req = ckalloc(sizeof(treq_RequestType));
    memset(req, 0, sizeof(treq_RequestType));

    req->curl_easy = curl_easy_init();
    if (req->curl_easy == NULL) {
        goto error;
    }

    // Set a buffer for cURL errors
    curl_easy_setopt(req->curl_easy, CURLOPT_ERRORBUFFER, req->curl_error);
    // Set a callback to save output data
    curl_easy_setopt(req->curl_easy, CURLOPT_WRITEFUNCTION, treq_write_callback);
    curl_easy_setopt(req->curl_easy, CURLOPT_WRITEDATA, (void *)req);
    // This data will be used when we get a callback from curl and we need
    // to know the corresponding treq_RequestType struct
    curl_easy_setopt(req->curl_easy, CURLOPT_PRIVATE, (void *)req);
    // Set our callback for debug messages
    curl_easy_setopt(req->curl_easy, CURLOPT_DEBUGFUNCTION, treq_debug_callback);
    curl_easy_setopt(req->curl_easy, CURLOPT_DEBUGDATA, (void *)req);


    req->allow_redirects = 1;

    req->state = TREQ_REQUEST_CREATED;

    DBG2(printf("return: %p", (void *)req));
    return req;

error:
    treq_RequestFree(req);
    DBG2(printf("return: ERROR (failed to alloc)"));
    return NULL;

}

void treq_RequestFree(treq_RequestType *req) {

    DBG2(printf("enter; req: %p", (void *)req));

    // If we have a callback event bound to this request, make sure
    // the callback event does not use a freed request.
    if (req->callback_event != NULL) {
        req->callback_event->request = NULL;
    }

    if (req->session != NULL) {
        treq_SessionRemoveRequest(req);
    }
    if (req->pool != NULL) {
        treq_PoolRemoveRequest(req);
    }

    if (req->curl_easy != NULL) {
        curl_easy_cleanup(req->curl_easy);
    }

    if (req->curl_headers != NULL) {
        curl_slist_free_all(req->curl_headers);
    }

    if (req->curl_mime != NULL) {
        curl_mime_free(req->curl_mime);
    }

    if (req->auth != NULL) {
        treq_RequestAuthFree(req->auth);
    }

    if (req->content != NULL) {
        ckfree(req->content);
    }

    Tcl_FreeObject(req->cmd_name);
    Tcl_FreeObject(req->url);
    Tcl_FreeObject(req->headers);
    Tcl_FreeObject(req->callback);
    Tcl_FreeObject(req->callback_debug);
    Tcl_FreeObject(req->custom_method);
    Tcl_FreeObject(req->error);
    Tcl_FreeObject(req->content_type);
    Tcl_FreeObject(req->content_charset);
    Tcl_FreeObject(req->form);
    Tcl_FreeObject(req->header_accept);
    Tcl_FreeObject(req->header_content_type);

    if (req->cmd_token != NULL) {
        req->isDead = 1;
        Tcl_DeleteCommandFromToken(req->interp, req->cmd_token);
    }

    ckfree(req);

    DBG2(printf("return: ok"));
    return;

}

