/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TREQUESTS_COMMON_H
#define TREQUESTS_COMMON_H

#ifdef USE_NAVISERVER
#include "ns.h"
#else
#include <tcl.h>
#endif

#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <curl/curl.h>

#ifndef TCL_SIZE_MAX
typedef int Tcl_Size;
# define Tcl_GetSizeIntFromObj Tcl_GetIntFromObj
# define Tcl_NewSizeIntObj Tcl_NewIntObj
# define TCL_SIZE_MAX      INT_MAX
# define TCL_SIZE_MODIFIER ""
#endif

#ifndef Tcl_BounceRefCount
#define Tcl_BounceRefCount(x) Tcl_IncrRefCount((x));Tcl_DecrRefCount((x))
#endif

#if !defined(INT2PTR) && !defined(PTR2INT)
#   if defined(HAVE_INTPTR_T) || defined(INTPTR_MAX)
#       define INT2PTR(p) ((void *)(intptr_t)(p))
#       define PTR2INT(p) ((int)(intptr_t)(p))
#   else
#       define INT2PTR(p) ((void *)(p))
#       define PTR2INT(p) ((int)(p))
#   endif
#endif

#ifndef STRINGIFY
#  define STRINGIFY(x) STRINGIFY1(x)
#  define STRINGIFY1(x) #x
#endif

#define UNUSED(expr) do { (void)(expr); } while (0)

#ifdef DEBUG
# define DBG(x) x

#ifndef __FUNCTION_NAME__
    #if defined(__STDC_VERSION__)
        #if __STDC_VERSION__ >= 199901L
            #define __FUNCTION_NAME__ __func__
        #elif defined(__GNUC__) && __GNUC__ >= 2
            #define __FUNCTION_NAME__ __FUNCTION__
        #endif
    #elif defined(_MSC_VER)
        #define __FUNCTION_NAME__ __FUNCTION__
    #else
        #define __FUNCTION_NAME__ "<unknown func>"
    #endif
#endif

# define DBG2(x) {printf("%s: ", __FUNCTION_NAME__); x; printf("\n"); fflush(stdout);}
#else
# define DBG(x)
# define DBG2(x)
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

#define CheckArgs(min,max,n,msg) \
                 if ((objc < min) || (objc >max)) { \
                     Tcl_WrongNumArgs(interp, n, objv, msg); \
                     return TCL_ERROR; \
                 }

#define SetResult(str) Tcl_ResetResult(interp); \
                     Tcl_SetStringObj(Tcl_GetObjResult(interp), (str), -1)

#define Tcl_FreeObject(obj) if ((obj) != NULL) { Tcl_DecrRefCount(obj); (obj) = NULL; }

#define TCL_TSD_INIT(keyPtr) \
    (ThreadSpecificData *)Tcl_GetThreadData((keyPtr), sizeof(ThreadSpecificData))

typedef struct treq_LinkedListType {
    void *item;
    struct treq_LinkedListType *next;
} treq_LinkedListType;

#define treq_LinkedListInsert(ll,li) (li)->next = (ll); (ll) = (li)
#define treq_LinkedListRemoveByItem(ll,i) \
    { \
        treq_LinkedListType *__prev = NULL; \
        for (treq_LinkedListType *__cur = (ll); __cur != NULL; __prev = __cur, __cur = __cur->next) { \
            if (__cur->item != (void *)(i)) continue; \
            if (__prev == NULL) { (ll) = __cur->next; } else { __prev->next = __cur->next; } \
            ckfree(__cur); \
            break; \
        } \
    }
#define treq_LinkedListNewItem(li,i) \
    treq_LinkedListType *(li) = ckalloc(sizeof(treq_LinkedListType)); \
    (li)->item = (void *)(i)
#define treq_LinkedListInsertNewItem(ll,i) \
    { \
        treq_LinkedListNewItem(__tmpitem, i); \
        treq_LinkedListInsert((ll), __tmpitem); \
    }
#define treq_LinkedListFree(ll) \
    for (treq_LinkedListType *__cur; (ll) != NULL; __cur = (ll), (ll) = (ll)->next, ckfree(__cur))

typedef struct treq_RequestType treq_RequestType;
typedef struct treq_SessionType treq_SessionType;
typedef struct treq_PoolType treq_PoolType;
typedef struct treq_RequestAuthType treq_RequestAuthType;

Tcl_Obj *treq_GenerateHeaderContentType(Tcl_Obj *data);
Tcl_Obj *treq_GenerateHeaderAccept(Tcl_Obj *data);

Tcl_Obj *treq_UrlencodeTclObject(Tcl_Obj *data);
Tcl_Obj *treq_UrldecodeTclObject(Tcl_Obj *data);
Tcl_Obj *treq_UrldecodeString(const char *data, Tcl_Size length);

#ifdef __cplusplus
extern "C" {
#endif

void treq_ParseContentType(const char *data, Tcl_Obj **type_ptr, Tcl_Obj **charset_ptr);
Tcl_Obj *treq_ConvertCharsetToEncoding(Tcl_Obj *charset);
void treq_ExecuteTclCallback(Tcl_Interp *interp, Tcl_Obj *callback, Tcl_Size objc, Tcl_Obj **objv, int background_error);

#ifdef __cplusplus
}
#endif

#endif // TREQUESTS_COMMON_H
