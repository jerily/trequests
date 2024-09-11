/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "library.h"
#include "treqSession.h"
#include "treqRequest.h"
#include "treqPool.h"

typedef struct treq_optionListType {
    Tcl_Obj *value;
    Tcl_Size count;
    int is_missing;
} treq_optionListType;

static int lappend_arg(void *clientData, Tcl_Obj *objPtr, void *dstPtr) {
    UNUSED(clientData);
    treq_optionListType *option_list = (treq_optionListType *)dstPtr;
    option_list->count++;
    if (objPtr == NULL) {
        option_list->is_missing = 1;
        if (option_list->value != NULL) {
            Tcl_DecrRefCount(option_list->value);
            option_list->value = NULL;
        }
    } else {
        if (option_list->value == NULL) {
            option_list->value = Tcl_NewListObj(1, &objPtr);
            Tcl_IncrRefCount(option_list->value);
        } else {
            Tcl_ListObjAppendElement(NULL, option_list->value, objPtr);
        }
    }
    return 1;
}

/*
static int copy_arg(void *clientData, Tcl_Obj *objPtr, void *dstPtr) {
    UNUSED(clientData);
    if (objPtr == NULL) {
        *((void **)dstPtr) = INT2PTR(1);
    } else {
        *((Tcl_Obj **)dstPtr) = objPtr;
    }
    return 1;
}
*/

static int treq_OptionListMergeDicts(Tcl_Interp *interp, treq_optionListType *option_list) {

    DBG2(printf("enter"));

    int rc = TCL_OK;

    Tcl_Size objc;
    Tcl_Obj **objv;
    Tcl_ListObjGetElements(interp, option_list->value, &objc, &objv);

    // Make sure that the first element is a valid dict
    Tcl_Obj *result = objv[0];
    Tcl_Size dict_size;
    if (Tcl_DictObjSize(interp, result, &dict_size) != TCL_OK) {
        result = NULL;
        DBG2(printf("return: ERROR (invalid first element)"));
        goto error;
    }

    // If we have only one element, then return it.
    if (objc < 2) {
        DBG2(printf("return: ok (only the 1st element)"));
        goto done;
    }

    // Duplicate the result object, as we will be modifying it by adding values
    // from other elements.
    result = Tcl_DuplicateObj(result);

    for (Tcl_Size i = 1; i < objc; i++) {

        DBG2(printf("add element #%" TCL_SIZE_MODIFIER "d", i));

        Tcl_DictSearch search;
        Tcl_Obj *key, *value;
        int done;

        if (Tcl_DictObjFirst(interp, objv[i], &search, &key, &value, &done) != TCL_OK) {
            Tcl_BounceRefCount(result);
            result = NULL;
            DBG2(printf("return: ERROR (not a valid dict)"));
            goto error;
        }

        for (; !done ; Tcl_DictObjNext(&search, &key, &value, &done)) {
            Tcl_DictObjPut(NULL, result, key, value);
        }

        Tcl_DictObjDone(&search);

    }

    // We will use dict_size below to use NULL in case of an empty dict
    Tcl_DictObjSize(NULL, result, &dict_size);

    DBG2(printf("return: ok"));

    goto done;

error:
    rc = TCL_ERROR;

done:
    if (result != NULL) {
        if (dict_size != 0) {
            Tcl_IncrRefCount(result);
        } else {
            Tcl_BounceRefCount(result);
            result = NULL;
        }
    }
    Tcl_DecrRefCount(option_list->value);
    option_list->value = result;

    return rc;

}

// This function merges two lists and returns a new one with no duplicates.
// It can compare keys in a case-insensitive manner. But it will work only for
// ascii keys. We use this functionality for headers and expect there
// to be only ascii keys.
static Tcl_Obj *treq_MergeDicts(Tcl_Obj *dict1, Tcl_Obj *dict2, int case_insensitive) {

    DBG2(printf("enter; dict1: %p dict2: %p", (void *)dict1, (void *)dict2));

    if (dict1 == NULL) {
        dict1 = Tcl_NewDictObj();
    } else {
        dict1 = Tcl_DuplicateObj(dict1);
    }

    Tcl_DictSearch search1, search2;
    Tcl_Obj *key1, *key2, *value2;
    int done1, done2;
    const char *key2_str;

    Tcl_DictObjFirst(NULL, dict2, &search2, &key2, &value2, &done2);
    for (; !done2 ; Tcl_DictObjNext(&search2, &key2, &value2, &done2)) {

        key2_str = Tcl_GetString(key2);
        key1 = NULL;

        DBG2(printf("push key [%s] value [%s]", key2_str, Tcl_GetString(value2)));

        if (case_insensitive) {
            Tcl_DictObjFirst(NULL, dict1, &search1, &key1, NULL, &done1);
            for (; !done1 ; Tcl_DictObjNext(&search1, &key1, NULL, &done1)) {
                if (curl_strequal(Tcl_GetString(key1), key2_str)) {
                    break;
                }
                DBG2(printf("not a duplicate: [%s]", Tcl_GetString(key1)));
                key1 = NULL;
            }
            Tcl_DictObjDone(&search1);

            // If key1 is not NULL, then it is a found duplicate key
            if (key1 != NULL) {
                DBG2(printf("found duplicate: [%s]", Tcl_GetString(key1)));
                Tcl_DictObjRemove(NULL, dict1, key1);
            }
        }

        Tcl_DictObjPut(NULL, dict1, key2, value2);

    }
    Tcl_DictObjDone(&search2);

    DBG2(printf("return: ok"));
    return dict1;

}

static Tcl_Command treq_CreateObjCommand(Tcl_Interp *interp, const char *cmd_template,
    Tcl_ObjCmdProc *proc, ClientData clientData, Tcl_CmdDeleteProc *deleteProc)
{
    char buf[128];
    snprintf(buf, sizeof(buf), cmd_template, (void*)clientData);
    SetResult(buf);
    return Tcl_CreateObjCommand(interp, buf, proc, clientData, deleteProc);
}

static int treq_ValidateOptions(Tcl_Interp *interp,
    treq_optionListType *opt_headers,
    treq_optionListType *opt_data_form)
{

    const char *option_name;

    if (opt_headers == NULL) {
        goto skipHeaders;
    }

    option_name = "-headers";

    if (opt_headers->is_missing) {
        DBG2(printf("return: ERROR (no arg for %s)", option_name));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("\"%s\" option requires"
            " an additional argument", option_name));
        return TCL_ERROR;
    }

    if (opt_headers->count == 0) {
        DBG2(printf("headers: <none>"));
        goto skipHeaders;
    }

    if (treq_OptionListMergeDicts(interp, opt_headers) != TCL_OK) {
        DBG2(printf("return: ERROR (%s error: %s)", option_name, Tcl_GetStringResult(interp)));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s option is expected to be"
            " a valid dict, but got: %s", option_name, Tcl_GetStringResult(interp)));
        return TCL_ERROR;
    }

    if (opt_headers->value == NULL) {
        DBG2(printf("headers dict is empty"));
    } else {
        DBG2(printf("headers: [%s]", Tcl_GetString(opt_headers->value)));
    }

skipHeaders:

    if (opt_data_form == NULL) {
        goto skipDataForm;
    }

    option_name = "-data_form";

    if (opt_data_form->is_missing) {
        DBG2(printf("return: ERROR (no arg for %s)", option_name));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("\"%s\" option requires"
            " an additional argument", option_name));
        return TCL_ERROR;
    }

    if (opt_data_form->count == 0) {
        DBG2(printf("data form: <none>"));
        goto skipDataForm;
    }

    if (treq_OptionListMergeDicts(interp, opt_data_form) != TCL_OK) {
        DBG2(printf("return: ERROR (%s error: %s)", option_name, Tcl_GetStringResult(interp)));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s option is expected to be"
            " a valid dict, but got: %s", option_name, Tcl_GetStringResult(interp)));
        return TCL_ERROR;
    }

    if (opt_data_form->value == NULL) {
        DBG2(printf("data form dict is empty"));
    } else {
        DBG2(printf("data form: [%s]", Tcl_GetString(opt_data_form->value)));
    }

skipDataForm:

    DBG2(printf("return: ok"));
    return TCL_OK;

}

static int treq_RequestHandleCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    treq_RequestType *request = (treq_RequestType *)clientData;

    DBG2(printf("enter: objc: %d", objc));

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ?args?");
        DBG2(printf("return: TCL_ERROR (wrong # args)"));
        return TCL_ERROR;
    }

    static const char *const commands[] = {
        "text", "content", "error", "headers", "header", "encoding",
        "status_code",
        "destroy",
        NULL
    };

    enum commands {
        cmdText, cmdContent, cmdError, cmdHeaders, cmdHeader, cmdEncoding,
        cmdStatusCode,
        cmdDestroy
    };

    int command;
    if (Tcl_GetIndexFromObj(interp, objv[1], commands, "command", 0, &command) != TCL_OK) {
        return TCL_ERROR;
    }

    int rc = TCL_OK;
    Tcl_Obj *result = NULL;

    switch ((enum commands) command) {
    case cmdDestroy:
        DBG2(printf("destroy request"));
        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, "");
            DBG2(printf("return: TCL_ERROR (wrong # args)"));
            return TCL_ERROR;
        }
        Tcl_DeleteCommandFromToken(request->interp, request->cmd_token);
        break;
    case cmdContent:
        DBG2(printf("get content"));
        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, "");
            DBG2(printf("return: TCL_ERROR (wrong # args)"));
            return TCL_ERROR;
        }
        result = treq_RequestGetContent(request);
        break;
    case cmdText:
        DBG2(printf("get content"));
        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, "");
            DBG2(printf("return: TCL_ERROR (wrong # args)"));
            return TCL_ERROR;
        }
        result = treq_RequestGetText(request);
        break;
    case cmdError:
        DBG2(printf("get error"));
        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, "");
            DBG2(printf("return: TCL_ERROR (wrong # args)"));
            return TCL_ERROR;
        }
        result = treq_RequestGetError(request);
        break;
    case cmdHeaders:
        DBG2(printf("get headers"));
        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, "");
            DBG2(printf("return: TCL_ERROR (wrong # args)"));
            return TCL_ERROR;
        }
        result = treq_RequestGetHeaders(request);
        break;
    case cmdHeader:
        if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "header");
            DBG2(printf("return: TCL_ERROR (header, wrong # args)"));
            return TCL_ERROR;
        }
        DBG2(printf("get header: [%s]", Tcl_GetString(objv[2])));
        result = treq_RequestGetHeader(request, Tcl_GetString(objv[2]));
        if (result == NULL) {
            rc = TCL_ERROR;
            result = Tcl_Format(NULL, "there is no header \"%s\" in the"
                " server response", 1, &objv[2]);
        }
        break;
    case cmdEncoding:
        if (objc > 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "?encoding?");
            DBG2(printf("return: TCL_ERROR (encoding, wrong # args)"));
            return TCL_ERROR;
        }
        if (objc > 2) {
            DBG2(printf("set encoding: [%s]", Tcl_GetString(objv[2])));
            Tcl_Encoding encoding = Tcl_GetEncoding(interp, Tcl_GetString(objv[2]));
            if (encoding == NULL) {
                DBG2(printf("return: TCL_ERROR"));
                return TCL_ERROR;
            }
            treq_RequestSetEncoding(request, encoding);
        }
        result = treq_RequestGetEncoding(request);
        break;
    case cmdStatusCode:
        DBG2(printf("get status code"));
        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, "");
            DBG2(printf("return: TCL_ERROR (wrong # args)"));
            return TCL_ERROR;
        }
        result = treq_RequestGetStatusCode(request);
        break;
    }

    Tcl_SetObjResult(interp, (result == NULL ? Tcl_NewObj() : result));
    DBG2(printf("return: %s", (rc == TCL_OK ? "ok" : "ERROR")));
    return rc;

}

static void treq_RequestHandleDelete(ClientData clientData) {
    treq_RequestType *request = (treq_RequestType *)clientData;
    if (request->isDead) {
        DBG2(printf("already dead"));
        return;
    }
    DBG2(printf("enter..."));
    request->cmd_token = NULL;
    treq_RequestFree(request);
    DBG2(printf("return: ok"));
}

static int treq_CreateNewRequest(Tcl_Interp *interp, treq_RequestMethodType method, Tcl_Obj *custom_method,
    int objc, Tcl_Obj *const objv[], treq_SessionType *session)
{
    DBG2(printf("enter; objc: %d", objc));

    Tcl_Obj *url = objv[0];

    int rc = TCL_OK;

    treq_optionListType opt_headers = { NULL, 0, 0 };
    int opt_noredirect = 0;
    int opt_verbose = 0;
    treq_optionListType opt_data_form = { NULL, 0, 0 };

#pragma GCC diagnostic push
// ignore warning for copy_arg:
//     warning: ISO C forbids conversion of function pointer to object pointer type [-Wpedantic]
#pragma GCC diagnostic ignored "-Wpedantic"
    Tcl_ArgvInfo ArgTable[] = {
        { TCL_ARGV_FUNC,     "-headers",    lappend_arg, &opt_headers,    NULL, NULL },
        { TCL_ARGV_FUNC,     "-data_form",  lappend_arg, &opt_data_form,  NULL, NULL },
        { TCL_ARGV_CONSTANT, "-noredirect", INT2PTR(1),  &opt_noredirect, NULL, NULL },
        { TCL_ARGV_CONSTANT, "-verbose",    INT2PTR(1),  &opt_verbose,    NULL, NULL },
        TCL_ARGV_TABLE_END
    };
#pragma GCC diagnostic pop

    Tcl_Size temp_objc = objc;
    if (Tcl_ParseArgsObjv(interp, ArgTable, &temp_objc, objv, NULL) != TCL_OK) {
        DBG2(printf("return: ERROR (failed to parse args)"));
        goto error;
    }

    if (treq_ValidateOptions(interp, &opt_headers, &opt_data_form) != TCL_OK) {
        DBG2(printf("return: ERROR (failed to validate)"));
        goto error;
    }

    treq_RequestType *request;
    if (session == NULL) {
        request = treq_RequestInit();
    } else {
        request = treq_SessionRequestInit(session);
    }

    if (request == NULL) {
        SetResult("failed to alloc");
        DBG2(printf("return: ERROR (failed to alloc)"));
        goto error;
    }

    if (opt_headers.value != NULL) {
        Tcl_Obj *headers_template = (session == NULL ? NULL : session->request_template->headers);
        request->headers = treq_MergeDicts(headers_template, opt_headers.value, 1);
        Tcl_IncrRefCount(request->headers);
    }

    if (opt_data_form.value != NULL) {
        request->data_form = opt_data_form.value;
        Tcl_IncrRefCount(request->data_form);
    }

    request->url = url;
    Tcl_IncrRefCount(request->url);

    request->method = method;

    request->custom_method = custom_method;
    if (request->custom_method != NULL) {
        Tcl_IncrRefCount(request->custom_method);
    }

    if (opt_noredirect) {
        request->noredirect = opt_noredirect;
    }

    if (opt_verbose) {
        request->verbose = opt_verbose;
    }

    treq_RequestRun(request);

    request->interp = interp;
    request->cmd_token = treq_CreateObjCommand(interp, "::trequests::request::handler%p",
        treq_RequestHandleCmd, (ClientData)request, treq_RequestHandleDelete);

    goto done;

error:
    rc = TCL_ERROR;

done:
    if (opt_headers.value != NULL) {
        Tcl_DecrRefCount(opt_headers.value);
    }
    return rc;

}

static int treq_RequestCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    DBG2(printf("enter; custom request:%s objc: %d", (clientData == NULL ? "yes" : "no"), objc));

    int rc;

    if (clientData == NULL) {

        if (objc < 3) {
            Tcl_WrongNumArgs(interp, 1, objv, "method url ?options?");
            DBG2(printf("return: TCL_ERROR (wrong # args)"));
            return TCL_ERROR;
        }

        rc = treq_CreateNewRequest(interp, TREQ_METHOD_CUSTOM, objv[1], objc - 2, objv + 2, NULL);

    } else {

        if (objc < 2) {
            Tcl_WrongNumArgs(interp, 1, objv, "url ?options?");
            DBG2(printf("return: TCL_ERROR (wrong # args)"));
            return TCL_ERROR;
        }

        rc = treq_CreateNewRequest(interp, (treq_RequestMethodType)PTR2INT(clientData), NULL, objc - 1, objv + 1, NULL);

    }

    DBG2(printf("return: %s", (rc == TCL_OK ? "ok" : "ERROR")));
    return TCL_OK;

}

static int treq_SessionHandleCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    treq_SessionType *session = (treq_SessionType *)clientData;

    DBG2(printf("enter: objc: %d", objc));

    if (objc < 2) {
wrongNumArgs:
        Tcl_WrongNumArgs(interp, 1, objv, "method url ?options?");
        // Unfortunately, we do not have access to INTERP_ALTERNATE_WRONG_ARGS
        // from the extension. Let's simulate it.
        Tcl_AppendPrintfToObj(Tcl_GetObjResult(interp), " or \"%s request method url ?options?\"", Tcl_GetString(objv[0]));
        Tcl_AppendPrintfToObj(Tcl_GetObjResult(interp), " or \"%s destroy\"", Tcl_GetString(objv[0]));
        DBG2(printf("return: TCL_ERROR (wrong # args)"));
        return TCL_ERROR;
    }

    static const char *const commands[] = {
        "head", "get", "post", "put", "patch", "delete", "request",
        "destroy",
        NULL
    };

    enum commands {
        cmdHead, cmdGet, cmdPost, cmdPut, cmdPatch, cmdDelete, cmdRequest,
        cmdDestroy
    };

    int command;
    if (Tcl_GetIndexFromObj(interp, objv[1], commands, "command", 0, &command) != TCL_OK) {
        return TCL_ERROR;
    }

    int rc = TCL_OK;

    switch ((enum commands) command) {
    case cmdDestroy:
        DBG2(printf("destroy session"));
        if (objc != 2) {
            goto wrongNumArgs;
        }
        Tcl_DeleteCommandFromToken(session->interp, session->cmd_token);
        break;
    case cmdRequest:
        DBG2(printf("request command"));
        if (objc < 4) {
            goto wrongNumArgs;
        }
        rc = treq_CreateNewRequest(interp, (treq_RequestMethodType)command, objv[2], objc - 3, objv + 3, session);
        break;
    default:
        DBG2(printf("'%s' method", Tcl_GetString(objv[1])));
        if (objc < 3) {
            goto wrongNumArgs;
        }
        rc = treq_CreateNewRequest(interp, (treq_RequestMethodType)command, NULL, objc - 2, objv + 2, session);
        break;
    }

    DBG2(printf("return: %s", (rc == TCL_OK ? "ok" : "ERROR")));
    return rc;

}

static void treq_SessionHandleDelete(ClientData clientData) {
    treq_SessionType *session = (treq_SessionType *)clientData;
    DBG2(printf("enter..."));
    treq_SessionFree(session);
    DBG2(printf("return: ok"));
}

static int treq_SessionCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    UNUSED(clientData);

    DBG2(printf("enter; objc: %d", objc));

    int rc = TCL_OK;

    treq_optionListType opt_headers = { NULL, 0, 0 };

    int opt_noredirect = 0;
    int opt_verbose = 0;

#pragma GCC diagnostic push
// ignore warning for copy_arg:
//     warning: ISO C forbids conversion of function pointer to object pointer type [-Wpedantic]
#pragma GCC diagnostic ignored "-Wpedantic"
    Tcl_ArgvInfo ArgTable[] = {
        { TCL_ARGV_FUNC,     "-headers",    lappend_arg, &opt_headers,    NULL, NULL },
        { TCL_ARGV_CONSTANT, "-noredirect", INT2PTR(1),  &opt_noredirect, NULL, NULL },
        { TCL_ARGV_CONSTANT, "-verbose",    INT2PTR(1),  &opt_verbose,    NULL, NULL },
        TCL_ARGV_TABLE_END
    };
#pragma GCC diagnostic pop

    Tcl_Size temp_objc = objc;
    if (Tcl_ParseArgsObjv(interp, ArgTable, &temp_objc, objv, NULL) != TCL_OK) {
        DBG2(printf("return: ERROR (failed to parse args)"));
        goto error;
    }

    if (treq_ValidateOptions(interp, &opt_headers, NULL) != TCL_OK) {
        DBG2(printf("return: ERROR (failed to validate)"));
        goto error;
    }

    treq_SessionType *session = treq_SessionInit();
    if (session == NULL) {
        SetResult("failed to alloc");
        DBG2(printf("return: ERROR (failed to alloc)"));
        goto error;
    }

    if (opt_headers.value != NULL) {
        session->request_template->headers = treq_MergeDicts(NULL, opt_headers.value, 1);
        Tcl_IncrRefCount(session->request_template->headers);
    }

    if (opt_noredirect) {
        session->request_template->noredirect = opt_noredirect;
    }

    if (opt_verbose) {
        session->request_template->verbose = opt_verbose;
    }

    session->interp = interp;
    session->cmd_token = treq_CreateObjCommand(interp, "::trequests::session::handler%p",
        treq_SessionHandleCmd, (ClientData)session, treq_SessionHandleDelete);

    DBG2(printf("return: ok (%s)", Tcl_GetStringResult(interp)));
    goto done;

error:
    rc = TCL_ERROR;

done:
    if (opt_headers.value != NULL) {
        Tcl_DecrRefCount(opt_headers.value);
    }
    return rc;

}


#if TCL_MAJOR_VERSION > 8
#define MIN_VERSION "9.0"
#else
#define MIN_VERSION "8.6"
#endif

static struct {
    int is_initialized;
    int is_shutdown;
    Tcl_Mutex init_mx;
} glob = { 0, 0, NULL };

static Tcl_ExitProc treq_PackageExitProc;
static Tcl_ExitProc treq_PackageThreadExitProc;

int Trequests_Init(Tcl_Interp *interp) {

    DBG2(printf("initialization; tid: %p", (void *)Tcl_GetCurrentThread()));

    if (Tcl_InitStubs(interp, MIN_VERSION, 0) == NULL) {
        SetResult("Unable to initialize Tcl stubs");
        return TCL_ERROR;
    }

    Tcl_MutexLock(&glob.init_mx);
    if (!glob.is_initialized) {
        DBG2(printf("initialize cURL"));
        curl_global_init(CURL_GLOBAL_DEFAULT);
        DBG2(printf("set exit handler"));
        Tcl_CreateExitHandler(treq_PackageExitProc, NULL);
        glob.is_initialized = 1;
    }
    Tcl_MutexUnlock(&glob.init_mx);

    DBG2(printf("set thread exit handler"));
    Tcl_CreateThreadExitHandler(treq_PackageThreadExitProc, NULL);

    Tcl_CreateNamespace(interp, "::trequests", NULL, NULL);
    Tcl_CreateNamespace(interp, "::trequests::session", NULL, NULL);
    Tcl_CreateNamespace(interp, "::trequests::request", NULL, NULL);

    Tcl_CreateObjCommand(interp, "::trequests::request", treq_RequestCmd, NULL, NULL);

    Tcl_CreateObjCommand(interp, "::trequests::head", treq_RequestCmd,
        INT2PTR((treq_RequestMethodType)TREQ_METHOD_HEAD), NULL);

    Tcl_CreateObjCommand(interp, "::trequests::get", treq_RequestCmd,
        INT2PTR((treq_RequestMethodType)TREQ_METHOD_GET), NULL);

    Tcl_CreateObjCommand(interp, "::trequests::post", treq_RequestCmd,
        INT2PTR((treq_RequestMethodType)TREQ_METHOD_POST), NULL);

    Tcl_CreateObjCommand(interp, "::trequests::put", treq_RequestCmd,
        INT2PTR((treq_RequestMethodType)TREQ_METHOD_PUT), NULL);

    Tcl_CreateObjCommand(interp, "::trequests::patch", treq_RequestCmd,
        INT2PTR((treq_RequestMethodType)TREQ_METHOD_PATCH), NULL);

    Tcl_CreateObjCommand(interp, "::trequests::delete", treq_RequestCmd,
        INT2PTR((treq_RequestMethodType)TREQ_METHOD_DELETE), NULL);

    Tcl_CreateObjCommand(interp, "::trequests::session", treq_SessionCmd, NULL, NULL);

    Tcl_RegisterConfig(interp, "trequests", treq_pkgconfig, "iso8859-1");

    DBG2(printf("return: ok"));
    return Tcl_PkgProvide(interp, "trequests", XSTR(VERSION));

}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *)Trequests_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif

static void treq_PackageThreadExitProc(ClientData clientData) {
    UNUSED(clientData);
    DBG2(printf("enter... tid: %p", (void *)Tcl_GetCurrentThread()));
    treq_SessionThreadExitProc();
    treq_PoolThreadExitProc();
    if (glob.is_shutdown) {
        DBG2(printf("shutdown cURL"));
        curl_global_cleanup();
        glob.is_initialized = 0;
    }
    DBG2(printf("return: ok"));
}

static void treq_PackageExitProc(ClientData clientData) {
    UNUSED(clientData);
    DBG2(printf("enter... tid: %p", (void *)Tcl_GetCurrentThread()));
    glob.is_shutdown = 1;
    DBG2(printf("return: ok"));
}