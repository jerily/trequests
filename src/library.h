/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TREQUESTS_LIBRARY_H
#define TREQUESTS_LIBRARY_H

#include "common.h"

static Tcl_Config const treq_pkgconfig[] = {
    { "package-version", XSTR(VERSION) },
    {NULL, NULL}
};

#ifdef __cplusplus
extern "C" {
#endif

EXTERN int Trequests_Init(Tcl_Interp *interp);
#ifdef USE_NAVISERVER
NS_EXTERN int Ns_ModuleVersion = 1;
NS_EXTERN int Ns_ModuleInit(const char *server, const char *module);
#endif

#ifdef __cplusplus
}
#endif

#endif // TREQUESTS_LIBRARY_H
