/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "treqPool.h"

struct treq_PoolType {
    CURLM *curl_multi;
};

typedef struct ThreadSpecificData {

    treq_PoolType *pool_default;

} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

treq_PoolType *treq_PoolInit(void) {

    DBG2(printf("enter..."));

    treq_PoolType *pool = ckalloc(sizeof(treq_PoolType));

    pool->curl_multi = curl_multi_init();
    if (pool->curl_multi == NULL) {
        ckfree(pool);
        DBG2(printf("return: ERROR (could not create a pool)"));
        return NULL;
    }

    DBG2(printf("return: ok"));
    return pool;

}

void treq_PoolFree(treq_PoolType *pool) {
    DBG2(printf("enter..."));
    curl_multi_cleanup(pool->curl_multi);
    ckfree(pool);
    DBG2(printf("return: ok"));
}


void treq_PoolAddRequest(void) {
    DBG2(printf("enter..."));
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

