/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TREQUESTS_TESTING_H
#define TREQUESTS_TESTING_H
#ifdef TREQUESTS_TESTING_MODE

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

Tcl_Obj *treq_RequestGetEasyOpts(treq_RequestType *req, Tcl_Obj *opt_name);
void treq_RequestRegisterEasyOption(treq_RequestType *req, const char *opt_name, ...);

#define treq_RequestRegisterEasyOption(req, opt_name, val) treq_RequestRegisterEasyOption(req, #opt_name, val)

#else // TREQUESTS_TESTING_MODE

#define treq_RequestRegisterEasyOption(req, opt_name, val) {}

#endif // TREQUESTS_TESTING_MODE

#endif // TREQUESTS_TESTING_H
