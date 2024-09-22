/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TREQUESTS_TREQREQUESTAUTH_H
#define TREQUESTS_TREQREQUESTAUTH_H

#include "common.h"

typedef struct treq_RequestAuthType {
    Tcl_Obj *username;
    Tcl_Obj *password;
    Tcl_Obj *token;
    Tcl_Obj *aws_sigv4;
    long scheme;
} treq_RequestAuthType;

#ifdef __cplusplus
extern "C" {
#endif

treq_RequestAuthType *treq_RequestAuthInit(Tcl_Obj *username, Tcl_Obj *password, Tcl_Obj *token, Tcl_Obj *aws_sigv4, long scheme);
void treq_RequestAuthFree(treq_RequestAuthType *auth);

int treq_RequestAuthParseOption(Tcl_Interp * interp, Tcl_Obj *data, long *result_ptr);

#define treq_RequestAuthDuplicate(auth) \
    treq_RequestAuthInit((auth)->username, (auth)->password, (auth)->token, (auth)->aws_sigv4, (auth)->scheme)

#ifdef __cplusplus
}
#endif

#endif // TREQUESTS_TREQREQUESTAUTH_H
