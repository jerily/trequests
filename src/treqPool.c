/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "treqPool.h"

typedef struct ThreadSpecificData {

    CURLM *pool;

} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

void treq_PoolThreadExitProc(void) {

    DBG2(printf("enter..."));

    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    if (tsdPtr->pool != NULL) {
        DBG2(printf("release pool"));
        curl_multi_cleanup(tsdPtr->pool);
        tsdPtr->pool = NULL;
    }

    DBG2(printf("return: ok"));

}

