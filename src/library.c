/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "library.h"
#include "treqSession.h"
#include "treqRequest.h"
#include "treqPool.h"
#include "treqRequestAuth.h"

typedef struct treq_optionCommonType {
    const char *name;
    int is_missing;
} treq_optionCommonType;

typedef struct treq_optionListType {
    const char *name;
    int is_missing;
    Tcl_Obj *value;
    Tcl_Size count;
} treq_optionListType;

typedef struct treq_optionDataType {
    const char *name;
    int is_missing;
    Tcl_Obj *value;
    Tcl_Size count;
    int is_urlencoded;
    int is_fields;
    int is_typed;
} treq_optionDataType;

typedef struct treq_optionBooleanType {
    const char *name;
    int is_missing;
    Tcl_Obj *raw;
    int value;
} treq_optionBooleanType;

typedef struct treq_optionObjectType {
    const char *name;
    int is_missing;
    Tcl_Obj *value;
} treq_optionObjectType;

typedef struct treq_optionAuthType {
    const char *name;
    int is_missing;
    Tcl_Obj *value;
    Tcl_Obj *username;
    Tcl_Obj *password;
} treq_optionAuthType;

typedef struct treq_optionAuthSchemeType {
    const char *name;
    int is_missing;
    Tcl_Obj *raw;
    long value;
} treq_optionAuthSchemeType;

typedef struct treq_optionAuthAwsSigv4Type {
    const char *name;
    int is_missing;
    Tcl_Obj *raw;
    Tcl_Obj *value;
} treq_optionAuthAwsSigv4Type;

typedef struct treq_RequestOptions {
    treq_optionListType headers;
    treq_optionListType form;
    treq_optionBooleanType verbose;
    treq_optionBooleanType allow_redirects;
    treq_optionObjectType callback;
    treq_optionObjectType callback_debug;
    treq_optionAuthSchemeType auth_scheme;
    treq_optionObjectType auth_token;
    treq_optionAuthType auth;
    treq_optionAuthAwsSigv4Type auth_aws_sigv4;
    treq_optionObjectType accept;
    treq_optionObjectType content_type;
    treq_optionDataType data;
    treq_optionDataType data_urlencode;
    treq_optionDataType data_fields;
    treq_optionDataType data_fields_urlencode;
    treq_optionDataType json;
    treq_optionDataType params;
    treq_optionDataType params_raw;
    treq_optionBooleanType verify;
    treq_optionBooleanType verify_host;
    treq_optionBooleanType verify_peer;
    treq_optionBooleanType verify_status;
    int async;
    int simple;
    int timeout;
    int timeout_connect;
} treq_RequestOptions;

#define treq_InitRequestOptions() { \
    .headers =                { "-headers",               -1, NULL, 0 }, \
    .form =                   { "-from",                  -1, NULL, 0 }, \
    .verbose =                { "-verbose",               -1, NULL, 0 }, \
    .allow_redirects =        { "-allow_redirects",       -1, NULL, 0 }, \
    .callback =               { "-callback",              -1, NULL }, \
    .callback_debug =         { "-callback_debug",        -1, NULL }, \
    .auth_scheme =            { "-auth_scheme",           -1, NULL, -1 }, \
    .auth_token =             { "-auth_token",            -1, NULL }, \
    .auth_aws_sigv4 =         { "-auth_aws_sigv4",        -1, NULL, NULL }, \
    .auth =                   { "-auth",                  -1, NULL, NULL, NULL }, \
    .accept =                 { "-accept",                -1, NULL }, \
    .content_type =           { "-content_type",          -1, NULL }, \
    .data =                   { "-data",                  -1, NULL, 0, 0, 0, 0 }, \
    .data_urlencode =         { "-data_urlencode",        -1, NULL, 0, 1, 0, 0 }, \
    .data_fields =            { "-data_fields",           -1, NULL, 0, 0, 1, 0 }, \
    .data_fields_urlencode =  { "-data_fields_urlencode", -1, NULL, 0, 1, 1, 0 }, \
    .json =                   { "-json",                  -1, NULL, 0, 0, 0, 1 }, \
    .params =                 { "-params",                -1, NULL, 0, 1, 1, 0 }, \
    .params_raw =             { "-params_raw",            -1, NULL, 0, 0, 0, 0 }, \
    .verify =                 { "-verify",                -1, NULL, -1 }, \
    .verify_host =            { "-verify_host",           -1, NULL, -1 }, \
    .verify_peer =            { "-verify_peer",           -1, NULL, -1 }, \
    .verify_status =          { "-verify_status",         -1, NULL, -1 }, \
    .async = 0, \
    .simple = 0, \
    .timeout = -1, \
    .timeout_connect = -1 \
}

#define treq_FreeRequestOptions(o) \
    Tcl_FreeObject((o).headers.value); \
    Tcl_FreeObject((o).form.value); \
    Tcl_FreeObject((o).auth_aws_sigv4.value); \
    Tcl_FreeObject((o).data.value); \
    Tcl_FreeObject((o).data_urlencode.value); \
    Tcl_FreeObject((o).data_fields.value); \
    Tcl_FreeObject((o).data_fields_urlencode.value); \
    Tcl_FreeObject((o).json.value); \
    Tcl_FreeObject((o).params.value); \
    Tcl_FreeObject((o).params_raw.value)

#define isOptionExists(opt) ((opt).is_missing == 0)

static int lappend_arg(void *clientData, Tcl_Obj *objPtr, void *dstPtr) {
    UNUSED(clientData);
    treq_optionListType *option_list = (treq_optionListType *)dstPtr;
    option_list->count++;
    if (objPtr == NULL) {
        option_list->is_missing = 1;
        Tcl_FreeObject(option_list->value);
    } else {
        if (option_list->value == NULL) {
            option_list->value = Tcl_NewListObj(1, &objPtr);
            Tcl_IncrRefCount(option_list->value);
        } else {
            Tcl_ListObjAppendElement(NULL, option_list->value, objPtr);
        }
        option_list->is_missing = 0;
    }
    return 1;
}

static int boolean_arg (void *clientData, Tcl_Obj *objPtr, void *dstPtr) {
    UNUSED(clientData);
    treq_optionBooleanType *option_boolean = (treq_optionBooleanType *)dstPtr;
    if (objPtr == NULL) {
        option_boolean->is_missing = 1;
    } else {
        option_boolean->is_missing = 0;
        option_boolean->raw = objPtr;
    }
    return 1;
}

static int object_arg(void *clientData, Tcl_Obj *objPtr, void *dstPtr) {
    UNUSED(clientData);
    treq_optionObjectType *option_object = (treq_optionObjectType *)dstPtr;
    if (objPtr == NULL) {
        option_object->is_missing = 1;
    } else {
        option_object->is_missing = 0;
        option_object->value = objPtr;
    }
    return 1;
}

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

static int treq_ValidateOptionCommon(Tcl_Interp *interp, treq_optionCommonType *data) {

    if (data->is_missing == 0) {
        return TCL_CONTINUE;
    }

    // Do we have missing value for the option?
    if (data->is_missing == 1) {
        DBG2(printf("return: ERROR (no arg for %s)", data->name));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("\"%s\" option requires"
            " an additional argument", data->name));
        return TCL_ERROR;
    }

    DBG2(printf("option %s: <none>", data->name));
    return TCL_OK;

}

#define VALIDATE_COMMON(d) \
    { \
        int __rc = treq_ValidateOptionCommon(interp, (treq_optionCommonType *)d); \
        if (__rc != TCL_CONTINUE) return __rc; \
    }

static int treq_ValidateOptionListOfDicts(Tcl_Interp *interp, treq_optionListType *data) {

    VALIDATE_COMMON(data);

    if (treq_OptionListMergeDicts(interp, data) != TCL_OK) {
        DBG2(printf("return: ERROR (%s error: %s)", data->name, Tcl_GetStringResult(interp)));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s option is expected to be"
            " a valid dict, but got: %s", data->name, Tcl_GetStringResult(interp)));
        return TCL_ERROR;
    }

    if (data->value == NULL) {
        DBG2(printf("option %s: <empty dict>", data->name));
    } else {
        DBG2(printf("option %s: [%s]", data->name, Tcl_GetString(data->value)));
    }

    return TCL_OK;

}

static int treq_ValidateOptionData(Tcl_Interp *interp, treq_optionDataType *data) {

    VALIDATE_COMMON(data);

    // If we are here, then we are sure that we have a value in the form of
    // a list that has at least 1 element in it

    Tcl_Size objc;
    Tcl_Obj **objv;
    Tcl_ListObjGetElements(NULL, data->value, &objc, &objv);

    // If we have typed data (i.e. json), make sure we only have one option
    if (data->is_typed) {

        if (data->count != 1) {
            DBG2(printf("return: ERROR (%s option specified %d times)", data->name, (int)data->count));
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s option specified multiple times", data->name));
            return TCL_ERROR;
        }

    }

    // Process elements somehow
    Tcl_Obj *value = NULL, *chunk;

    for (Tcl_Size i = 0; i < objc; i++) {

        if (data->is_fields) {

            // We have key-value pairs

            Tcl_Size sub_objc;
            Tcl_Obj **sub_objv;
            if (Tcl_ListObjGetElements(interp, objv[i], &sub_objc, &sub_objv) != TCL_OK) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("an invalid list is specified for"
                    " option %s: %s", data->name, Tcl_GetStringResult(interp)));
                goto error;
            }

            if (sub_objc == 0) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("an even numbered list is expected"
                    " for option %s, but got an empty list", data->name));
                goto error;
            }

            if ((sub_objc % 2) != 0) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("an even numbered list is expected"
                    " for option %s, but got a list of %d items", data->name, (int)sub_objc));
                goto error;
            }

            Tcl_Obj *key, *val;
            chunk = NULL;

            // Go throught the list and add key-value pairs
            for (Tcl_Size j = 0; j < sub_objc; j++) {

                key = (data->is_urlencoded ? treq_UrlencodeTclObject(sub_objv[j++]) : sub_objv[j++]);

                if (key == NULL) {
                    Tcl_SetObjResult(interp, Tcl_ObjPrintf("failed to urlencode the key"
                        " while processing the %s option", data->name));
                    if (chunk != NULL) {
                        Tcl_BounceRefCount(chunk);
                    }
                    goto error;
                }

                val = (data->is_urlencoded ? treq_UrlencodeTclObject(sub_objv[j]) : sub_objv[j]);

                if (val == NULL) {
                    Tcl_SetObjResult(interp, Tcl_ObjPrintf("failed to urlencode the value for"
                        " the key \"%s\" while processing the %s option", Tcl_GetString(sub_objv[--j]),
                        data->name));
                    if (chunk != NULL) {
                        Tcl_BounceRefCount(chunk);
                    }
                    Tcl_BounceRefCount(key);
                    goto error;
                }

                // If it is the first key-value pair, then initialize the chunk by the key
                if (chunk == NULL) {
                    chunk = Tcl_DuplicateObj(key);
                } else {
                    Tcl_AppendToObj(chunk, "&", 1);
                    Tcl_AppendObjToObj(chunk, key);
                }

                Tcl_BounceRefCount(key);

                Tcl_AppendToObj(chunk, "=", 1);
                Tcl_AppendObjToObj(chunk, val);
                Tcl_BounceRefCount(val);

            }

        } else if (data->is_urlencoded) {
            // We don't have key-value pairs, but we need to urlencode the value
            chunk = treq_UrlencodeTclObject(objv[i]);
            if (chunk == NULL) {
                Tcl_SetObjResult(interp, Tcl_ObjPrintf("failed to urlencode data"
                    " while processing the %s option", data->name));
            }
        } else {
            // We don't have key-value pairs and we should use the value as is
            chunk = objv[i];
        }

        // If chunk is NULL, it will be considered an error
        if (chunk == NULL) {
error:
            if (value != NULL) {
                Tcl_BounceRefCount(value);
            }
            return TCL_ERROR;
        }

        if (value == NULL) {
            value = chunk;
        } else {
            // Join the current chunk with the result using an ampersand
            Tcl_AppendToObj(value, "&", 1);
            Tcl_AppendObjToObj(value, chunk);
            Tcl_BounceRefCount(chunk);
        }

    }

    // Replace value for the option
    Tcl_DecrRefCount(data->value);
    data->value = value;
    Tcl_IncrRefCount(data->value);

    DBG2(printf("option %s: [%s]", data->name, Tcl_GetString(data->value)));
    return TCL_OK;

}

static int treq_ValidateOptionBoolean(Tcl_Interp *interp, treq_optionBooleanType *data) {

    VALIDATE_COMMON(data);

    if (Tcl_GetBooleanFromObj(NULL, data->raw, &data->value) != TCL_OK) {
        DBG2(printf("return: ERROR (%s is not bool, but '%s')", data->name, Tcl_GetString(data->raw)));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s option is expected to be"
            " a boolean, but got: '%s'", data->name, Tcl_GetString(data->raw)));
        return TCL_ERROR;
    }

    DBG2(printf("option %s: %s", data->name, (data->value ? "true" : "false")));

    return TCL_OK;

}

static int treq_ValidateOptionObjectList(Tcl_Interp *interp, treq_optionObjectType *data) {

    VALIDATE_COMMON(data);

    Tcl_Size list_length;
    if (Tcl_ListObjLength(interp, data->value, &list_length) != TCL_OK) {
        DBG2(printf("return: ERROR (%s is not a list, but '%s')", data->name, Tcl_GetString(data->value)));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s option is expected to be"
            " a list, but got: %s", data->name, Tcl_GetStringResult(interp)));
        return TCL_ERROR;
    }

    if (list_length == 0) {
        DBG2(printf("option %s: <empty list>", data->name));
        data->value = NULL;
    } else {
        DBG2(printf("option %s: [%s]", data->name, Tcl_GetString(data->value)));
    }

    return TCL_OK;

}

// The auth option is a list with 2 elements. But we don't want to use
// treq_ValidateOptionObjectList() to validate it because it contains
// security-sensitive data that should be shown in the debug log.
// Thus, we use separate procedure to validation it.
static int treq_ValidateOptionAuth(Tcl_Interp *interp, treq_optionAuthType *data) {

    VALIDATE_COMMON(data);

    Tcl_Size objc;
    Tcl_Obj **objv;
    if (Tcl_ListObjGetElements(interp, data->value, &objc, &objv) != TCL_OK) {
        DBG2(printf("return: ERROR (%s is not a list)", data->name));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s option is expected to be"
            " a list with 2 elements, but got: %s", data->name, Tcl_GetStringResult(interp)));
        return TCL_ERROR;
    }

    if (objc != 2) {
        DBG2(printf("return: ERROR (list length is %" TCL_SIZE_MODIFIER "d)", objc));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s option is expected to be"
            " a list with 2 elements, but got a list with %d elements", data->name,
            (int)objc));
        return TCL_ERROR;
    }

    data->username = objv[0];
    data->password = objv[1];

    DBG2(printf("option %s: username:[%s] password:[%s]", data->name,
        Tcl_GetString(data->username), "<hidden>"));

    return TCL_OK;

}

#define checkMutuallyExclusive(opt1,opt2) \
    if (isOptionExists(opt2)) { \
        Tcl_SetObjResult(interp, Tcl_ObjPrintf(error_mutually_exclusive, (opt1).name, (opt2).name)); \
        DBG2(printf("return: ERROR (%s)", Tcl_GetStringResult(interp))); \
        return TCL_ERROR; \
    }

static int treq_ValidateOptions(Tcl_Interp *interp, treq_RequestOptions *opt) {

    DBG2(printf("enter"));

    if (treq_ValidateOptionListOfDicts(interp, &opt->headers) != TCL_OK                                 ||
        treq_ValidateOptionListOfDicts(interp, &opt->form) != TCL_OK                                    ||
        treq_ValidateOptionObjectList(interp, &opt->callback) != TCL_OK                                 ||
        treq_ValidateOptionObjectList(interp, &opt->callback_debug) != TCL_OK                           ||
        treq_ValidateOptionObjectList(interp, (treq_optionObjectType *)&opt->auth_scheme) != TCL_OK     ||
        treq_ValidateOptionCommon(interp, (treq_optionCommonType *)&opt->auth_token) == TCL_ERROR       ||
        treq_ValidateOptionAuth(interp, &opt->auth) != TCL_OK                                           ||
        treq_ValidateOptionObjectList(interp, (treq_optionObjectType *)&opt->auth_aws_sigv4) != TCL_OK  ||
        treq_ValidateOptionCommon(interp, (treq_optionCommonType *)&opt->accept) == TCL_ERROR           ||
        treq_ValidateOptionCommon(interp, (treq_optionCommonType *)&opt->content_type) == TCL_ERROR     ||
        treq_ValidateOptionData(interp, &opt->data) != TCL_OK                                           ||
        treq_ValidateOptionData(interp, &opt->data_urlencode) != TCL_OK                                 ||
        treq_ValidateOptionData(interp, &opt->data_fields) != TCL_OK                                    ||
        treq_ValidateOptionData(interp, &opt->data_fields_urlencode) != TCL_OK                          ||
        treq_ValidateOptionData(interp, &opt->json) != TCL_OK                                           ||
        treq_ValidateOptionData(interp, &opt->params) != TCL_OK                                         ||
        treq_ValidateOptionData(interp, &opt->params_raw) != TCL_OK                                     ||
        treq_ValidateOptionBoolean(interp, &opt->verify) != TCL_OK                                      ||
        treq_ValidateOptionBoolean(interp, &opt->verify_host) != TCL_OK                                 ||
        treq_ValidateOptionBoolean(interp, &opt->verify_peer) != TCL_OK                                 ||
        treq_ValidateOptionBoolean(interp, &opt->verify_status) != TCL_OK                               ||
        treq_ValidateOptionBoolean(interp, &opt->verbose) != TCL_OK                                     ||
        treq_ValidateOptionBoolean(interp, &opt->allow_redirects) != TCL_OK)
    {
        return TCL_ERROR;
    }

    treq_optionDataType *opt_defined1, *opt_defined2;

    // Check for mutually exclusive options

    // Check for mutually exclusive data options. We will do this in 2 passes.
    // 1. Go through all data options and try to find the option that is defined
    // 2. Go through all data options and try to find the second defined option

    opt_defined1 =
        isOptionExists(opt->data)                  ? &opt->data                  :
        isOptionExists(opt->data_urlencode)        ? &opt->data_urlencode        :
        isOptionExists(opt->data_fields)           ? &opt->data_fields           :
        isOptionExists(opt->data_fields_urlencode) ? &opt->data_fields_urlencode :
        isOptionExists(opt->json)                  ? &opt->json                  :
        NULL;

    // The seconds pass

    if (opt_defined1 != NULL) {

        opt_defined2 =
            (opt_defined1 != &opt->data                  && isOptionExists(opt->data))                  ? &opt->data                  :
            (opt_defined1 != &opt->data_urlencode        && isOptionExists(opt->data_urlencode))        ? &opt->data_urlencode        :
            (opt_defined1 != &opt->data_fields           && isOptionExists(opt->data_fields))           ? &opt->data_fields           :
            (opt_defined1 != &opt->data_fields_urlencode && isOptionExists(opt->data_fields_urlencode)) ? &opt->data_fields_urlencode :
            (opt_defined1 != &opt->json                  && isOptionExists(opt->json))                  ? &opt->json                  :
            NULL;

        if (opt_defined2 != NULL) {
            goto mutuallyExclusiveError;
        }

    }

    if (isOptionExists(opt->params) && isOptionExists(opt->params_raw)) {
        opt_defined1 = &opt->params;
        opt_defined2 = &opt->params_raw;
        goto mutuallyExclusiveError;
    }

    if (isOptionExists(opt->auth_scheme) && treq_RequestAuthParseOption(interp, opt->auth_scheme.raw, &opt->auth_scheme.value) != TCL_OK) {
        return TCL_ERROR;
    }

    if (isOptionExists(opt->auth_aws_sigv4)) {

        Tcl_Size objc;
        Tcl_Obj **objv;
        Tcl_ListObjGetElements(NULL, opt->auth_aws_sigv4.raw, &objc, &objv);

        if (objc < 1 || objc > 4) {
            DBG2(printf("return: ERROR (auth_aws_sigv4 list length is %" TCL_SIZE_MODIFIER "d)", objc));
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s option is expected to be"
                " a list with number of elements from 1 to 4, but got a list"
                " with %d elements", opt->auth_aws_sigv4.name, (int)objc));
            return TCL_ERROR;
        }

        opt->auth_aws_sigv4.value = Tcl_NewListObj(0, NULL);
        Tcl_IncrRefCount(opt->auth_aws_sigv4.value);

        for (Tcl_Size i = 0; i < objc; i++) {
            if (i != 0) {
                Tcl_AppendToObj(opt->auth_aws_sigv4.value, ":", 1);
            }
            Tcl_AppendObjToObj(opt->auth_aws_sigv4.value, objv[i]);
        }

        DBG2(printf("computed option %s: %s", opt->auth_aws_sigv4.name, Tcl_GetString(opt->auth_aws_sigv4.value)));

    }

    if (opt->async && opt->simple) {
        SetResult("-async and -simple switches are incompatible with each other");
        return TCL_ERROR;
    }

    if (isOptionExists(opt->auth_token)) {
        DBG2(printf("option %s: %s", opt->auth_token.name, "<hidden>"));
    }

    DBG2(printf("option %s: %s", "-simple", (opt->simple ? "true" : "false")));
    DBG2(printf("option %s: %s", "-async", (opt->async ? "true" : "false")));
    DBG2(printf("option %s: %d", "-timeout", opt->timeout));
    DBG2(printf("option %s: %d", "-timeout_connect", opt->timeout_connect));

    DBG2(printf("return: ok"));
    return TCL_OK;

mutuallyExclusiveError:

    Tcl_SetObjResult(interp, Tcl_ObjPrintf("mutually exclusive options"
        " %s and %s were specified", opt_defined1->name, opt_defined2->name));
    DBG2(printf("return: ERROR (%s)", Tcl_GetStringResult(interp)));
    return TCL_ERROR;

}

static int treq_RequestHandleCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    treq_RequestType *request = (treq_RequestType *)clientData;

    DBG2(printf("enter: objc: %d", objc));

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ?args?");
        DBG2(printf("return: TCL_ERROR (wrong # args)"));
        return TCL_ERROR;
    }

    static const struct {
        const char *name;
        treq_RequestGetterProc *proc;
        int arg_min;
        int arg_max;
        const char *arg_help;
    } commands[] = {
        { "text",        treq_RequestGetText,       2, 2, NULL         },
        { "content",     treq_RequestGetContent,    2, 2, NULL         },
        { "error",       treq_RequestGetError,      2, 2, NULL         },
        { "headers",     treq_RequestGetHeaders,    2, 2, NULL         },
        { "header",      NULL,                      3, 3, "header"     },
        { "encoding",    treq_RequestGetEncoding,   2, 3, "?encoding?" },
        { "status_code", treq_RequestGetStatusCode, 2, 2, NULL         },
        { "state",       treq_RequestGetState,      2, 2, NULL         },
        { "destroy",     NULL,                      2, 2, NULL         },
        { NULL, NULL, 0, 0, NULL }
    };

    // The elements of the enumeration must be arranged in the same order
    // as in the commands struct
    enum commands {
        cmdText, cmdContent, cmdError, cmdHeaders, cmdHeader, cmdEncoding,
        cmdStatusCode, cmdState,
        cmdDestroy
    };

    int command;
    if (Tcl_GetIndexFromObjStruct(interp, objv[1], commands, sizeof(commands[0]), "command", 0, &command) != TCL_OK) {
        return TCL_ERROR;
    }

    DBG2(printf("command [%s]", commands[command].name));

    if ((objc < commands[command].arg_min) || (objc > commands[command].arg_max)) {
        Tcl_WrongNumArgs(interp, 2, objv, commands[command].arg_help);
        DBG2(printf("return: TCL_ERROR (wrong # args)"));
        return TCL_ERROR;
    }

    int rc = TCL_OK;
    Tcl_Obj *result = NULL;

    switch ((enum commands) command) {
    case cmdDestroy:
        Tcl_DeleteCommandFromToken(request->interp, request->cmd_token);
        break;
    case cmdHeader:
        DBG2(printf("get header: [%s]", Tcl_GetString(objv[2])));
        result = treq_RequestGetHeader(request, Tcl_GetString(objv[2]));
        if (result == NULL) {
            rc = TCL_ERROR;
            result = Tcl_Format(NULL, "there is no header \"%s\" in the"
                " server response", 1, &objv[2]);
        }
        break;
    case cmdEncoding:
        if (objc > 2) {
            DBG2(printf("set encoding: [%s]", Tcl_GetString(objv[2])));
            Tcl_Encoding encoding = Tcl_GetEncoding(interp, Tcl_GetString(objv[2]));
            if (encoding == NULL) {
                DBG2(printf("return: TCL_ERROR"));
                return TCL_ERROR;
            }
            treq_RequestSetEncoding(request, encoding);
        }
        result = commands[command].proc(request);
        break;
    case cmdText:
    case cmdContent:
    case cmdError:
    case cmdHeaders:
    case cmdStatusCode:
    case cmdState:
        result = commands[command].proc(request);
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

#define SetRequestProperty(o,v) \
    (o) = (v); \
    if ((o) != NULL) { Tcl_IncrRefCount(o); }

#define GetSessionProperty(prop,default) \
    (request->session != NULL ? (request->session->prop) : (default))

static int treq_CreateNewRequest(Tcl_Interp *interp, treq_RequestMethodType method, Tcl_Obj *custom_method,
    int objc, Tcl_Obj *const objv[], treq_SessionType *session)
{
    DBG2(printf("enter; objc: %d", objc));

    Tcl_Obj *url = objv[0];

    int rc = TCL_OK;

    treq_RequestOptions opt = treq_InitRequestOptions();

#pragma GCC diagnostic push
// ignore warning for copy_arg:
//     warning: ISO C forbids conversion of function pointer to object pointer type [-Wpedantic]
#pragma GCC diagnostic ignored "-Wpedantic"
    Tcl_ArgvInfo ArgTable[] = {
        { TCL_ARGV_FUNC, "-headers",               lappend_arg, &opt.headers,               NULL, NULL },
        { TCL_ARGV_FUNC, "-form",                  lappend_arg, &opt.form,                  NULL, NULL },
        { TCL_ARGV_FUNC, "-allow_redirects",       boolean_arg, &opt.allow_redirects,       NULL, NULL },
        { TCL_ARGV_FUNC, "-verbose",               boolean_arg, &opt.verbose,               NULL, NULL },
        { TCL_ARGV_CONSTANT, "-async",             INT2PTR(1),  &opt.async,                 NULL, NULL },
        { TCL_ARGV_CONSTANT, "-simple",            INT2PTR(1),  &opt.simple,                NULL, NULL },
        { TCL_ARGV_FUNC, "-callback",              object_arg,  &opt.callback,              NULL, NULL },
        { TCL_ARGV_FUNC, "-callback_debug",        object_arg,  &opt.callback_debug,        NULL, NULL },
        { TCL_ARGV_FUNC, "-auth",                  object_arg,  &opt.auth,                  NULL, NULL },
        { TCL_ARGV_FUNC, "-auth_token",            object_arg,  &opt.auth_token,            NULL, NULL },
        { TCL_ARGV_FUNC, "-auth_scheme",           object_arg,  &opt.auth_scheme,           NULL, NULL },
        { TCL_ARGV_FUNC, "-auth_aws_sigv4",        object_arg,  &opt.auth_aws_sigv4,        NULL, NULL },
        { TCL_ARGV_FUNC, "-accept",                object_arg,  &opt.accept,                NULL, NULL },
        { TCL_ARGV_FUNC, "-content_type",          object_arg,  &opt.content_type,          NULL, NULL },
        { TCL_ARGV_FUNC, "-data",                  lappend_arg, &opt.data,                  NULL, NULL },
        { TCL_ARGV_FUNC, "-data_urlencode",        lappend_arg, &opt.data_urlencode,        NULL, NULL },
        { TCL_ARGV_FUNC, "-data_fields",           lappend_arg, &opt.data_fields,           NULL, NULL },
        { TCL_ARGV_FUNC, "-data_fields_urlencode", lappend_arg, &opt.data_fields_urlencode, NULL, NULL },
        { TCL_ARGV_FUNC, "-json",                  lappend_arg, &opt.json,                  NULL, NULL },
        { TCL_ARGV_FUNC, "-params",                lappend_arg, &opt.params,                NULL, NULL },
        { TCL_ARGV_FUNC, "-params_raw",            lappend_arg, &opt.params_raw,            NULL, NULL },
        { TCL_ARGV_INT,  "-timeout",               NULL,        &opt.timeout,               NULL, NULL },
        { TCL_ARGV_INT,  "-timeout_connect",       NULL,        &opt.timeout_connect,       NULL, NULL },
        { TCL_ARGV_FUNC, "-verify",                boolean_arg, &opt.verify,                NULL, NULL },
        { TCL_ARGV_FUNC, "-verify_host",           boolean_arg, &opt.verify_host,           NULL, NULL },
        { TCL_ARGV_FUNC, "-verify_peer",           boolean_arg, &opt.verify_peer,           NULL, NULL },
        { TCL_ARGV_FUNC, "-verify_status",         boolean_arg, &opt.verify_status,           NULL, NULL },
        TCL_ARGV_TABLE_END
    };
#pragma GCC diagnostic pop

    Tcl_Size temp_objc = objc;
    if (Tcl_ParseArgsObjv(interp, ArgTable, &temp_objc, objv, NULL) != TCL_OK) {
        DBG2(printf("return: ERROR (failed to parse args)"));
        goto error;
    }

    if (treq_ValidateOptions(interp, &opt) != TCL_OK) {
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

    SetRequestProperty(request->headers, isOptionExists(opt.headers) ?
        treq_MergeDicts(request->session->headers, opt.headers.value, 1) :
        GetSessionProperty(headers, NULL));

    if (opt.async) {
        SetRequestProperty(request->callback, isOptionExists(opt.callback) ?
            opt.callback.value :
            GetSessionProperty(callback, NULL));
    }

    SetRequestProperty(request->callback_debug, isOptionExists(opt.callback_debug) ?
        opt.callback_debug.value :
        GetSessionProperty(callback_debug, NULL));

    SetRequestProperty(request->form, opt.form.value);

    if (isOptionExists(opt.auth_scheme) || isOptionExists(opt.auth_token) || isOptionExists(opt.auth) || isOptionExists(opt.auth_aws_sigv4)) {

        request->auth = treq_RequestAuthInit(
            opt.auth.username, opt.auth.password,
            opt.auth_token.value,
            opt.auth_aws_sigv4.value,
            opt.auth_scheme.value);

    } else if (request->session != NULL && request->session->auth != NULL) {
        request->auth = treq_RequestAuthDuplicate(request->session->auth);
    }

    Tcl_Obj *accept =
        isOptionExists(opt.accept) ? opt.accept.value :
        isOptionExists(opt.json) ? Tcl_NewStringObj("json", -1) :
        (request->session != NULL && request->session->accept != NULL) ? request->session->accept :
        NULL;

    if (accept != NULL) {
        request->header_accept = treq_GenerateHeaderAccept(accept);
        Tcl_IncrRefCount(request->header_accept);
        Tcl_BounceRefCount(accept);
    }

    Tcl_Obj *content_type =
        isOptionExists(opt.content_type) ? opt.content_type.value :
        isOptionExists(opt.json) ? Tcl_NewStringObj("json", -1) :
        (request->session != NULL && request->session->content_type != NULL) ? request->session->content_type :
        NULL;

    if (content_type != NULL) {
        request->header_content_type = treq_GenerateHeaderContentType(content_type);
        Tcl_IncrRefCount(request->header_content_type);
        Tcl_BounceRefCount(content_type);
    }

    request->postfields =
        isOptionExists(opt.data)                  ? opt.data.value                  :
        isOptionExists(opt.data_urlencode)        ? opt.data_urlencode.value        :
        isOptionExists(opt.data_fields)           ? opt.data_fields.value           :
        isOptionExists(opt.data_fields_urlencode) ? opt.data_fields_urlencode.value :
        isOptionExists(opt.json)                  ? opt.json.value                  :
        NULL;

    if (request->postfields != NULL) {
        Tcl_IncrRefCount(request->postfields);
    }

    request->querystring =
        isOptionExists(opt.params)     ? opt.params.value     :
        isOptionExists(opt.params_raw) ? opt.params_raw.value :
        NULL;

    if (request->querystring != NULL) {
        Tcl_IncrRefCount(request->querystring);
    }

    request->verify_host =
        isOptionExists(opt.verify_host) ? opt.verify_host.value :
        isOptionExists(opt.verify) ? opt.verify.value :
        request->session == NULL ? -1 :
        request->session->verify_host != -1 ? request->session->verify_host :
        request->session->verify;

    request->verify_peer =
        isOptionExists(opt.verify_peer) ? opt.verify_peer.value :
        isOptionExists(opt.verify) ? opt.verify.value :
        request->session == NULL ? -1 :
        request->session->verify_peer != -1 ? request->session->verify_peer :
        request->session->verify;

    request->verify_status =
        isOptionExists(opt.verify_status) ? opt.verify_status.value :
        request->session != NULL ? request->session->verify_status :
        -1;

    SetRequestProperty(request->url, url);

    request->method = method;
    SetRequestProperty(request->custom_method, custom_method);

    request->allow_redirects =
        isOptionExists(opt.allow_redirects) ? opt.allow_redirects.value :
        (request->session != NULL && request->session->allow_redirects != -1) ? request->session->allow_redirects :
        1;

    request->verbose =
        isOptionExists(opt.verbose) ? opt.verbose.value :
        (request->session != NULL && request->session->verbose != -1) ? request->session->verbose :
        0;

    request->timeout =
        opt.timeout != -1 ? opt.timeout :
        request->session != NULL ? request->session->timeout :
        -1;

    request->timeout_connect =
        opt.timeout_connect != -1 ? opt.timeout_connect :
        request->session != NULL ? request->session->timeout_connect :
        -1;

    request->async = opt.async;

    request->interp = interp;

    treq_RequestRun(request);

    if (opt.simple) {

        switch (request->state) {
        case TREQ_REQUEST_DONE:
            Tcl_SetObjResult(interp, treq_RequestGetText(request));
            break;
        case TREQ_REQUEST_ERROR:
            Tcl_SetObjResult(interp, treq_RequestGetError(request));
            rc = TCL_ERROR;
            break;
        case TREQ_REQUEST_CREATED:
        case TREQ_REQUEST_INPROGRESS:
            SetResult("request is in wrong state");
            break;
        }

        treq_RequestFree(request);

        goto done;

    }

    request->cmd_token = treq_CreateObjCommand(interp, "::trequests::request::handler%p",
        treq_RequestHandleCmd, (ClientData)request, treq_RequestHandleDelete);

    request->cmd_name = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(request->cmd_name);

    goto done;

error:
    rc = TCL_ERROR;

done:
    treq_FreeRequestOptions(opt);
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

    treq_RequestOptions opt = treq_InitRequestOptions();

#pragma GCC diagnostic push
// ignore warning for copy_arg:
//     warning: ISO C forbids conversion of function pointer to object pointer type [-Wpedantic]
#pragma GCC diagnostic ignored "-Wpedantic"
    Tcl_ArgvInfo ArgTable[] = {
        { TCL_ARGV_FUNC, "-headers",         lappend_arg, &opt.headers,         NULL, NULL },
        { TCL_ARGV_FUNC, "-allow_redirects", boolean_arg, &opt.allow_redirects, NULL, NULL },
        { TCL_ARGV_FUNC, "-verbose",         boolean_arg, &opt.verbose,         NULL, NULL },
        { TCL_ARGV_FUNC, "-callback",        object_arg,  &opt.callback,        NULL, NULL },
        { TCL_ARGV_FUNC, "-callback_debug",  object_arg,  &opt.callback_debug,  NULL, NULL },
        { TCL_ARGV_FUNC, "-auth",            object_arg,  &opt.auth,            NULL, NULL },
        { TCL_ARGV_FUNC, "-auth_token",      object_arg,  &opt.auth_token,      NULL, NULL },
        { TCL_ARGV_FUNC, "-auth_scheme",     object_arg,  &opt.auth_scheme,     NULL, NULL },
        { TCL_ARGV_FUNC, "-auth_aws_sigv4",  object_arg,  &opt.auth_aws_sigv4,  NULL, NULL },
        { TCL_ARGV_FUNC, "-accept",          object_arg,  &opt.accept,          NULL, NULL },
        { TCL_ARGV_FUNC, "-content_type",    object_arg,  &opt.content_type,    NULL, NULL },
        { TCL_ARGV_INT,  "-timeout",         NULL,        &opt.timeout,         NULL, NULL },
        { TCL_ARGV_INT,  "-timeout_connect", NULL,        &opt.timeout_connect, NULL, NULL },
        { TCL_ARGV_FUNC, "-verify",          boolean_arg, &opt.verify,          NULL, NULL },
        { TCL_ARGV_FUNC, "-verify_host",     boolean_arg, &opt.verify_host,     NULL, NULL },
        { TCL_ARGV_FUNC, "-verify_peer",     boolean_arg, &opt.verify_peer,     NULL, NULL },
        { TCL_ARGV_FUNC, "-verify_status",   boolean_arg, &opt.verify_status,   NULL, NULL },
        TCL_ARGV_TABLE_END
    };
#pragma GCC diagnostic pop

    Tcl_Size temp_objc = objc;
    if (Tcl_ParseArgsObjv(interp, ArgTable, &temp_objc, objv, NULL) != TCL_OK) {
        DBG2(printf("return: ERROR (failed to parse args)"));
        goto error;
    }

    if (treq_ValidateOptions(interp, &opt) != TCL_OK) {
        DBG2(printf("return: ERROR (failed to validate)"));
        goto error;
    }

    treq_SessionType *session = treq_SessionInit();
    if (session == NULL) {
        SetResult("failed to alloc");
        DBG2(printf("return: ERROR (failed to alloc)"));
        goto error;
    }

    if (isOptionExists(opt.headers)) {
        session->headers = treq_MergeDicts(NULL, opt.headers.value, 1);
        Tcl_IncrRefCount(session->headers);
    }

    if (isOptionExists(opt.callback)) {
        session->callback = opt.callback.value;
        Tcl_IncrRefCount(session->callback);
    }

    if (isOptionExists(opt.callback_debug)) {
        session->callback_debug = opt.callback_debug.value;
        Tcl_IncrRefCount(session->callback_debug);
    }

    if (isOptionExists(opt.auth_scheme) || isOptionExists(opt.auth_token) || isOptionExists(opt.auth) || isOptionExists(opt.auth_aws_sigv4)) {
        session->auth = treq_RequestAuthInit(
            opt.auth.username, opt.auth.password,
            opt.auth_token.value,
            opt.auth_aws_sigv4.value,
            opt.auth_scheme.value);
    }

    if (isOptionExists(opt.accept)) {
        session->accept = opt.accept.value;
        Tcl_IncrRefCount(session->accept);
    }

    if (isOptionExists(opt.content_type)) {
        session->content_type = opt.content_type.value;
        Tcl_IncrRefCount(session->content_type);
    }

    session->verify = isOptionExists(opt.verify) ? opt.verify.value : -1;
    session->verify_host = isOptionExists(opt.verify_host) ? opt.verify_host.value : -1;
    session->verify_peer = isOptionExists(opt.verify_peer) ? opt.verify_peer.value : -1;
    session->verify_status = isOptionExists(opt.verify_status) ? opt.verify_status.value : -1;

    session->allow_redirects = isOptionExists(opt.allow_redirects) ? opt.allow_redirects.value : -1;
    session->verbose = isOptionExists(opt.verbose) ? opt.verbose.value : -1;
    session->timeout = opt.timeout;
    session->timeout_connect = opt.timeout_connect;

    session->interp = interp;
    session->cmd_token = treq_CreateObjCommand(interp, "::trequests::session::handler%p",
        treq_SessionHandleCmd, (ClientData)session, treq_SessionHandleDelete);

    DBG2(printf("return: ok (%s)", Tcl_GetStringResult(interp)));
    goto done;

error:
    rc = TCL_ERROR;

done:
    treq_FreeRequestOptions(opt);
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
