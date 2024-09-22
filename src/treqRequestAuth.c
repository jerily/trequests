/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "treqRequestAuth.h"

int treq_RequestAuthParseOption(Tcl_Interp *interp, Tcl_Obj *data, long *result_ptr) {

    DBG2(printf("enter"));

    long result = 0;

    static const char *const options[] = {
        "any", "anysafe",
        "only",
        "basic", "digest", "digest_ie", "bearer", "negotiate", "ntlm", "aws_sigv4",
        NULL
    };

    enum options {
        optAny, optAnysafe,
        optOnly,
        optBasic, optDigest, optDigest_ie, optBearer, optNegotiate, optNtlm, optAws_sigv4
    };

    Tcl_Size objc;
    Tcl_Obj **objv;
    Tcl_ListObjGetElements(NULL, data, &objc, &objv);

    if (objc == 0) {
        DBG2(printf("return: ERROR (empty list)"));
        SetResult("an empty list cannot be specified as an authentication scheme");
        return TCL_ERROR;
    }

    // The rules are:
    //
    // 1. options "any" and "anysafe" are allowed if they are the only value
    // 2. option "only" is available if one scheme is specified. List size is expected
    //    to be 2 in this case (option + "only"). However, we should catch the case
    //    when "only" option is specified twice.

    for (Tcl_Size i = 0; i < objc; i++) {

        int idx;
        if (Tcl_GetIndexFromObj(interp, objv[i], options, "auth scheme option", 0, &idx) != TCL_OK) {
            DBG2(printf("return: ERROR (unknown option [%s])", Tcl_GetString(objv[i])));
            return TCL_ERROR;
        }

        DBG2(printf("check option: [%s]", Tcl_GetString(objv[i])));

        switch ((enum options)idx) {
        case optAny:
        case optAnysafe:
            if (objc != 1) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("\"%s\" option is"
                    " allowed if it is the only value", options[idx]));
                DBG2(printf("return: ERROR (not the only value)"));
                return TCL_ERROR;
            }
            result = ((enum options)idx == optAny ? CURLAUTH_ANY : CURLAUTH_ANYSAFE);
            break;
        case optOnly:
            if (objc != 2) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("\"%s\" option can be specified"
                    " in conjunction with one specific authentication value", options[idx]));
                DBG2(printf("return: ERROR (wrong number of options when 'only' is specified)"));
                return TCL_ERROR;
            } else if (i == 1 && result == CURLAUTH_ONLY) {
                // The previous value is "only" also
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("\"%s\" option specified twice", options[idx]));
                DBG2(printf("return: ERROR (double 'only' specified)"));
                return TCL_ERROR;
            }
            result |= CURLAUTH_ONLY;
            break;
        case optBasic:
            result |= CURLAUTH_BASIC;
            break;
        case optDigest:
            result |= CURLAUTH_DIGEST;
            break;
        case optDigest_ie:
            result |= CURLAUTH_DIGEST_IE;
            break;
        case optBearer:
            result |= CURLAUTH_BEARER;
            break;
        case optNegotiate:
            result |= CURLAUTH_NEGOTIATE;
            break;
        case optNtlm:
            result |= CURLAUTH_NTLM;
            break;
        case optAws_sigv4:
            result |= CURLAUTH_AWS_SIGV4;
            break;
        }

    }

    *result_ptr = result;
    DBG2(printf("return: ok (%ld)", result));
    return TCL_OK;

}

treq_RequestAuthType *treq_RequestAuthInit(Tcl_Obj *username, Tcl_Obj *password, Tcl_Obj *token, Tcl_Obj *aws_sigv4, long scheme) {

    DBG2(printf("enter"));

    treq_RequestAuthType *auth = ckalloc(sizeof(treq_RequestAuthType));
    memset(auth, 0, sizeof(treq_RequestAuthType));

    if (username != NULL) {
        DBG2(printf("username: [%s]", Tcl_GetString(username)));
        auth->username = username;
        Tcl_IncrRefCount(username);
    } else {
        DBG2(printf("username: [%s]", "<none>"));
    }

    if (password != NULL) {
        DBG2(printf("password: [%s]", "<hidden>"));
        auth->password = password;
        Tcl_IncrRefCount(password);
    } else {
        DBG2(printf("password: [%s]", "<none>"));
    }

    if (token != NULL) {
        DBG2(printf("token: [%s]", "<hidden>"));
        auth->token = token;
        Tcl_IncrRefCount(token);
    } else {
        DBG2(printf("token: [%s]", "<none>"));
    }

    if (aws_sigv4 != NULL) {
        DBG2(printf("aws_sigv4: [%s]", Tcl_GetString(aws_sigv4)));
        auth->aws_sigv4 = aws_sigv4;
        Tcl_IncrRefCount(aws_sigv4);
    } else {
        DBG2(printf("aws_sigv4: [%s]", "<none>"));
    }

    auth->scheme = scheme;
    DBG2(printf("scheme: %ld", scheme));

    DBG2(printf("return: ok"));
    return auth;

}

void treq_RequestAuthFree(treq_RequestAuthType *auth) {
    Tcl_FreeObject(auth->username);
    Tcl_FreeObject(auth->password);
    Tcl_FreeObject(auth->token);
    Tcl_FreeObject(auth->aws_sigv4);
    ckfree(auth);
}
