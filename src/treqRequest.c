/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "treqRequest.h"
#include "treqSession.h"

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

    Tcl_Obj *result = Tcl_NewDictObj();

    struct curl_header *prev = NULL;
    struct curl_header *h;

    while((h = curl_easy_nextheader(req->curl_easy, CURLH_HEADER, 0, prev))) {
        Tcl_DictObjPut(NULL, result, Tcl_NewStringObj(h->name, -1), Tcl_NewStringObj(h->value, -1));
        prev = h;
    }

    return result;

}

Tcl_Obj *treq_RequestGetHeader(treq_RequestType *req, const char *header) {

    struct curl_header *h;

    if (curl_easy_header(req->curl_easy, header, 0, CURLH_HEADER, -1, &h) != CURLHE_OK) {
        return NULL;
    }

    return Tcl_NewStringObj(h->value, -1);

}

static void treq_RequestSetError(treq_RequestType *req, Tcl_Obj *error) {

    if (req->error != NULL) {
        Tcl_DecrRefCount(req->error);
    }

    DBG2(printf("set request ERROR: [%s]", Tcl_GetString(error)));
    req->error = error;
    Tcl_IncrRefCount(req->error);
    req->state = TREQ_REQUEST_ERROR;

}

static size_t treq_write_callback(const char *ptr, size_t size, size_t nmemb, void *userdata) {

    treq_RequestType *req = (treq_RequestType *)userdata;
    size = size * nmemb;

    DBG2(printf("enter; existing size: %" TCL_SIZE_MODIFIER "d add size: %zu", req->content_size, size));

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
/*
    if (req->data_form != NULL) {

        DBG2(printf("add form data..."));

        req->curl_mime = curl_mime_init(req->curl_easy);
        curl_mimepart *field = NULL;

        Tcl_DictSearch search;
        Tcl_Obj *key, *value;
        int done;

        Tcl_DictObjFirst(NULL, req->data_form, &search, &key, &value, &done);
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
*/
    switch (req->method) {
    case TREQ_METHOD_HEAD:
        safe_curl_easy_setopt(CURLOPT_HTTPGET, 1L);
        safe_curl_easy_setopt(CURLOPT_NOBODY, 1L);
        break;
    case TREQ_METHOD_GET:
        safe_curl_easy_setopt(CURLOPT_HTTPGET, 1L);
        break;
    case TREQ_METHOD_POST:
        safe_curl_easy_setopt(CURLOPT_POST, 1L);
        break;
    case TREQ_METHOD_PUT:
        safe_curl_easy_setopt(CURLOPT_UPLOAD, 1L);
        break;
    case TREQ_METHOD_PATCH:
        safe_curl_easy_setopt(CURLOPT_CUSTOMREQUEST, "PATCH");
        break;
    case TREQ_METHOD_DELETE:
        safe_curl_easy_setopt(CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    case TREQ_METHOD_CUSTOM:
        safe_curl_easy_setopt(CURLOPT_CUSTOMREQUEST, Tcl_GetString(req->custom_method));
        break;
    }

    safe_curl_easy_setopt(CURLOPT_FOLLOWLOCATION, (req->noredirect ? 0L : 1L));
    safe_curl_easy_setopt(CURLOPT_VERBOSE, (req->verbose ? 1L : 0L));

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

        safe_curl_easy_setopt(CURLOPT_HTTPHEADER, req->curl_headers);

    }

    // Disable cURL misbehavior in the same cases. See:
    //     * https://curl.se/mail/lib-2017-07/0013.html
    //     * https://gms.tf/when-curl-sends-100-continue.html
    //     * https://stackoverflow.com/questions/49670008/how-to-disable-expect-100-continue-in-libcurl
    //DBG2(printf("disable Expect: 100-continue"));
    //req->curl_headers = curl_slist_append(req->curl_headers, "Expect:");

    DBG2(printf("run cURL request..."));
    req->state = TREQ_REQUEST_STARTED;
    CURLcode res = curl_easy_perform(req->curl_easy);

    if (res == CURLE_OK) {
        req->state = TREQ_REQUEST_DONE;
        DBG2(printf("request: ok"));
    } else {
        req->state = TREQ_REQUEST_ERROR;
        DBG2(printf("request: ERROR (%s)", Tcl_GetString(treq_RequestGetError(req))));
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

    if (req->session != NULL) {
        treq_SessionRemoveRequest(req);
    }

    if (req->curl_easy != NULL) {
        curl_easy_cleanup(req->curl_easy);
    }
    if (req->curl_headers != NULL) {
        curl_slist_free_all(req->curl_headers);
    }

    if (req->url != NULL) {
        Tcl_DecrRefCount(req->url);
    }
    if (req->headers != NULL) {
        Tcl_DecrRefCount(req->headers);
    }
    if (req->custom_method != NULL) {
        Tcl_DecrRefCount(req->custom_method);
    }
    if (req->error != NULL) {
        Tcl_DecrRefCount(req->error);
    }
    if (req->content != NULL) {
        ckfree(req->content);
    }
    if (req->content_type != NULL) {
        Tcl_DecrRefCount(req->content_type);
    }
    if (req->content_charset != NULL) {
        Tcl_DecrRefCount(req->content_charset);
    }
    if (req->data_form != NULL) {
        Tcl_DecrRefCount(req->data_form);
    }
    if (req->curl_mime != NULL) {
        curl_mime_free(req->curl_mime);
    }

    if (req->cmd_token != NULL) {
        req->isDead = 1;
        Tcl_DeleteCommandFromToken(req->interp, req->cmd_token);
    }

    ckfree(req);

    DBG2(printf("return: ok"));
    return;

}
