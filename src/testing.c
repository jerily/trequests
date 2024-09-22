/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "common.h"
#include "treqRequest.h"

typedef enum {
    TREQ_OPT_STRING,
    TREQ_OPT_LONG,
    TREQ_OPT_POINTER,
    TREQ_OPT_SLIST
} treq_OptionType;

static const struct {
    const char *name;
    treq_OptionType type;
} options[] = {
    { "CURLOPT_HTTPGET",           TREQ_OPT_LONG    },
    { "CURLOPT_NOBODY",            TREQ_OPT_LONG    },
    { "CURLOPT_HTTPGET",           TREQ_OPT_LONG    },
    { "CURLOPT_POST",              TREQ_OPT_LONG    },
    { "CURLOPT_UPLOAD",            TREQ_OPT_LONG    },
    { "CURLOPT_CUSTOMREQUEST",     TREQ_OPT_STRING  },
    { "CURLOPT_CURLU",             TREQ_OPT_POINTER },
    { "CURLOPT_POSTFIELDSIZE",     TREQ_OPT_LONG    },
    { "CURLOPT_POSTFIELDS",        TREQ_OPT_STRING  },
    { "CURLOPT_USERNAME",          TREQ_OPT_STRING  },
    { "CURLOPT_PASSWORD",          TREQ_OPT_STRING  },
    { "CURLOPT_XOAUTH2_BEARER",    TREQ_OPT_STRING  },
    { "CURLOPT_AWS_SIGV4",         TREQ_OPT_STRING  },
    { "CURLOPT_HTTPAUTH",          TREQ_OPT_LONG    },
    { "CURLOPT_MIMEPOST",          TREQ_OPT_POINTER },
    { "CURLOPT_FOLLOWLOCATION",    TREQ_OPT_LONG    },
    { "CURLOPT_VERBOSE",           TREQ_OPT_LONG    },
    { "CURLOPT_HTTPHEADER",        TREQ_OPT_SLIST   },
    { "CURLOPT_CONNECTTIMEOUT_MS", TREQ_OPT_LONG    },
    { "CURLOPT_TIMEOUT_MS",        TREQ_OPT_LONG    },
    { "CURLOPT_SSL_VERIFYHOST",    TREQ_OPT_LONG    },
    { "CURLOPT_SSL_VERIFYPEER",    TREQ_OPT_LONG    },
    { "CURLOPT_SSL_VERIFYSTATUS",  TREQ_OPT_LONG    },
    /* curl_url */
    { "CURLUPART_URL",             TREQ_OPT_STRING  },
    { "CURLUPART_QUERY",           TREQ_OPT_STRING  },
    { NULL, 0 }
};

Tcl_Obj *treq_RequestGetEasyOpts(treq_RequestType *req, Tcl_Obj *opt_name) {

    if (req->set_options == NULL) {
        return NULL;
    }

    if (opt_name == NULL) {
        return req->set_options;
    }

    Tcl_Obj *val = NULL;
    Tcl_DictObjGet(NULL, req->set_options, opt_name, &val);

    return val;

}

void treq_RequestRegisterEasyOption(treq_RequestType *req, const char *opt_name, ...) {

    DBG2(printf("add opt [%s]", opt_name));

    Tcl_Obj *opt_obj = Tcl_NewStringObj(opt_name, -1);

    int idx;
    if (Tcl_GetIndexFromObjStruct(NULL, opt_obj, options, sizeof(options[0]), NULL, TCL_EXACT, &idx) != TCL_OK) {
        Tcl_Panic("treq_RequestRegisterEasyOption(): unknown option to register \"%s\"", opt_name);
    }

    if (req->set_options == NULL) {
        req->set_options = Tcl_NewDictObj();
        Tcl_IncrRefCount(req->set_options);
    }

    Tcl_Obj *val;

    va_list arg;
    va_start(arg, opt_name);
    switch (options[idx].type) {
    case TREQ_OPT_STRING:
        val = Tcl_NewStringObj((const char *)va_arg(arg, char *), -1);
        break;
    case TREQ_OPT_LONG:
        val = Tcl_NewWideIntObj((long)va_arg(arg, long));
        break;
    case TREQ_OPT_POINTER:
        val = Tcl_NewStringObj("pointer", -1);
        break;
    case TREQ_OPT_SLIST:
        val = Tcl_NewListObj(0, NULL);
        struct curl_slist *list = (struct curl_slist *)va_arg(arg, void *);
        for (; list != NULL; list = list->next) {
            Tcl_ListObjAppendElement(NULL, val, Tcl_NewStringObj((const char *)list->data, -1));
        }
        break;
    }
    va_end(arg);

    Tcl_DictObjPut(NULL, req->set_options, opt_obj, val);
    Tcl_BounceRefCount(opt_obj);

}
