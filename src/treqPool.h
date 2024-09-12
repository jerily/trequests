/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TREQUESTS_TREQPOOL_H
#define TREQUESTS_TREQPOOL_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

void treq_PoolThreadExitProc(void);
int treq_PoolAddRequest(treq_RequestType *req);
void treq_PoolRemoveRequest(treq_RequestType *req);

#ifdef __cplusplus
}
#endif

#endif // TREQUESTS_TREQPOOL_H
