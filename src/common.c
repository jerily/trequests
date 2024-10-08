/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "common.h"
// for isdigit() / isspace()
#include <ctype.h>

// This constant is not defined in public cURL headers
#define CURL_MAX_INPUT_LENGTH 8000000

typedef struct shortcutType {
    const char *shortcut;
    const char *value;
} shortcutType;

static const shortcutType accept_shortcuts[] = {
    { "all", "*/*" },
    { "json", "application/json" },
    { NULL, NULL }
};

static const shortcutType content_type_shortcuts[] = {
    { "json", "application/json" },
    { NULL, NULL }
};

static const char *treq_GenerateHeader(Tcl_Obj *data, const shortcutType *table) {
    const char *value;
    int idx;
    if (Tcl_GetIndexFromObjStruct(NULL, data, table, sizeof(shortcutType), NULL, TCL_EXACT, &idx) == TCL_OK) {
        value = table[idx].value;
        DBG2(printf("convert [%s] to [%s]", Tcl_GetString(data), value));
    } else {
        value = Tcl_GetString(data);
        DBG2(printf("use [%s] as is", Tcl_GetString(data)));
    }
    return value;
}

Tcl_Obj *treq_GenerateHeaderAccept(Tcl_Obj *data) {
    return Tcl_ObjPrintf("Accept: %s", treq_GenerateHeader(data, accept_shortcuts));
}

Tcl_Obj *treq_GenerateHeaderContentType(Tcl_Obj *data) {
    return Tcl_ObjPrintf("Content-Type: %s", treq_GenerateHeader(data, content_type_shortcuts));
}

Tcl_Obj *treq_UrldecodeString(const char *data, Tcl_Size length) {

    // curl_easy_unescape() can handle up to INT_MAX bytes.
    // It is tricky to decode input string by chunks as we have to ensure that
    // we are passing in a chunk that is not in the middle of the encoded
    // character (%XX). Thus, currently we return an error (NULL) if the input
    // data size exceeds this value. Perhaps in the future this function
    // will be modified to handle larger values.
    if (length > INT_MAX) {
        return NULL;
    }

    int outlength;
    char *data_decoded = curl_easy_unescape(NULL, data, (int)length, &outlength);

    if (data_decoded == NULL) {
        return NULL;
    }

    Tcl_Obj *result = Tcl_NewStringObj(data_decoded, outlength);
    curl_free(data_decoded);

    return result;

}

Tcl_Obj *treq_UrldecodeTclObject(Tcl_Obj *data) {
    Tcl_Size data_length;
    const char *data_str = Tcl_GetStringFromObj(data, &data_length);
    return treq_UrldecodeString(data_str, data_length);
}

// This function is required for urlencode objects because curl_easy_escape()
// has an input size limit of CURL_MAX_INPUT_LENGTH.
Tcl_Obj *treq_UrlencodeTclObject(Tcl_Obj *data) {

    Tcl_Size data_length;
    const char *data_str = Tcl_GetStringFromObj(data, &data_length);

    Tcl_Obj *result = Tcl_NewObj();

    Tcl_Size pos = 0, bytes_left = data_length;
    while (bytes_left != 0) {

        int bytes_to_encode = (bytes_left > CURL_MAX_INPUT_LENGTH ? CURL_MAX_INPUT_LENGTH : bytes_left);

        char *data_encoded = curl_easy_escape(NULL, &data_str[pos], bytes_to_encode);

        if (data_encoded == NULL) {
            Tcl_BounceRefCount(result);
            return NULL;
        }

        Tcl_AppendToObj(result, data_encoded, -1);
        curl_free(data_encoded);

        pos += bytes_to_encode;
        bytes_left -= bytes_to_encode;

    }

    return result;

}

void treq_ExecuteTclCallback(Tcl_Interp *interp, Tcl_Obj *callback, Tcl_Size objc, Tcl_Obj **objv, int background_error) {

    DBG2(printf("enter, objc: %" TCL_SIZE_MODIFIER "d", objc));

    Tcl_Obj *cmd;

    if (objc > 0) {

        // Make a duplicate of callback, since we will be modifying it, by adding
        // arguments to the end of it
        cmd = Tcl_DuplicateObj(callback);
        Tcl_IncrRefCount(cmd);

        Tcl_Size length;
        // We need to know the length of cmd to add list elements using Tcl_ListObjReplace()
        Tcl_ListObjLength(NULL, cmd, &length);

        // Append arguments to the callback command
        Tcl_ListObjReplace(NULL, cmd, length, 0, objc, objv);

    } else {
        cmd = callback;
    }

    // Prepare objc/objv for Tcl_EvalObjv()
    Tcl_ListObjGetElements(NULL, cmd, &objc, &objv);

    // Save interp state
    Tcl_Preserve(interp);
    Tcl_InterpState state = Tcl_SaveInterpState(interp, TCL_OK);

    // Execute the callback
    DBG2(printf("run Tcl callback..."));
    int rc = Tcl_EvalObjv(interp, objc, objv, 0);
    DBG2(printf("return code is %s", (rc == TCL_OK ? "TCL_OK" : (rc == TCL_ERROR ? "TCL_ERROR" : "OTHER VALUE"))));

    // Don't touch cmd if it the same object as callback
    if (cmd != callback) {
        Tcl_DecrRefCount(cmd);
    }

    // If we got something wrong, report it using background error handler
    if (background_error && rc != TCL_OK) {
        Tcl_BackgroundException(interp, rc);
    }

    // Restore interp state
    Tcl_RestoreInterpState(interp, state);
    Tcl_Release(interp);

    DBG2(printf("return: ok"));

}


Tcl_Obj *treq_ConvertCharsetToEncoding(Tcl_Obj *charset) {

    DBG2(printf("enter, charset: [%s]", Tcl_GetString(charset)));

    const char *ptr = Tcl_GetStringFromObj(charset, NULL);
    Tcl_Obj *result = NULL;

    if (strncmp(ptr, "iso", 3) == 0) {

        // Skip first 3 chars
        ptr += 3;
        // Skip possible dash
        if (*ptr == '-') ptr++;

        if (strncmp(ptr, "8859-", 5) == 0) {

            // Skip match
            ptr += 5;
            // Check that other chars are digits
            for (const char *tmp = ptr; *tmp != '\0'; tmp++) {
                if (!isdigit(*ptr)) {
                    goto done;
                }
            }

            result = Tcl_NewStringObj("iso8859-", -1);
            Tcl_AppendToObj(result, ptr, -1);

        } else if (strncmp(ptr, "2022-", 5) == 0) {

            // Skip match
            ptr += 5;
            if (strcmp(ptr, "jp") != 0 || strcmp(ptr, "kr") != 0) {
                goto done;
            }

            result = Tcl_NewStringObj("iso2022-", -1);
            Tcl_AppendToObj(result, ptr, -1);

        } else if (strncmp(ptr, "lat", 3) == 0) {
            goto latin;
        } else {
            goto done;
        }

    } else if (strncmp(ptr, "shift", 5) == 0) {

        // Skip possible '-' and '_'
        if (*ptr == '-' || *ptr == '_') ptr++;
        // Check for 'jis'
        if (strcmp(ptr, "jis") != 0) {
            goto done;
        }

        result = Tcl_NewStringObj("shiftjis", -1);

    } else if (strncmp(ptr, "windows", 7) == 0 || strncmp(ptr, "cp", 2) == 0) {

        // Skip match
        ptr += (*ptr == 'w' ? 7 : 2);
        // Skip possible dash
        if (*ptr == '-') ptr++;

        // Check that other chars are digits
        for (const char *tmp = ptr; *tmp != '\0'; tmp++) {
            if (!isdigit(*ptr)) {
                goto done;
            }
        }

        result = Tcl_NewStringObj("cp", -1);
        Tcl_AppendToObj(result, ptr, -1);

    } else if (strcmp(ptr, "us-ascii") == 0) {

        result = Tcl_NewStringObj("ascii", -1);

    } else if (strncmp(ptr, "lat", 3) == 0) {

latin:
        // Skip match
        ptr += 3;
        // Skip possible 'in'
        if (strncmp(ptr, "in", 2) == 0) ptr += 2;
        // Skip possible dash
        if (*ptr == '-') ptr++;

        // Check that other chars are digits
        for (const char *tmp = ptr; *tmp != '\0'; tmp++) {
            if (!isdigit(*ptr)) {
                goto done;
            }
        }

        if (strcmp(ptr, "5") == 0) {
            result = Tcl_NewStringObj("iso8859-9", -1);
        } else if (strcmp(ptr, "1") == 0 || strcmp(ptr, "2") == 0 || strcmp(ptr, "3") == 0) {
            result = Tcl_NewStringObj("iso8859-", -1);
            Tcl_AppendToObj(result, ptr, -1);
        } else {
            result = Tcl_NewStringObj("iso8859-1", -1);
        }

    }

done:

    if (result == NULL) {
        DBG2(printf("could not convert encoding, charset will be used"));
        result = charset;
    }

    DBG2(printf("return: ok ([%s])", Tcl_GetString(result)));

    return result;

}

void treq_ParseContentType(const char *data, Tcl_Obj **type_ptr, Tcl_Obj **charset_ptr) {

    DBG2(printf("enter, data: [%s]", data));

    const char *ptr;

    char type_buf[128];
    size_t type_len = 0;

    char charset_buf[32];
    size_t charset_len = 0;

    for (ptr = data; *ptr != '\0'; ptr++) {

        if (isspace(*ptr)) {
            continue;
        }

        if (*ptr == ';') {
            ptr++;
            break;
        }

        if (type_len < sizeof(type_buf)) {
            type_buf[type_len++] = tolower(*ptr);
        }

    }

    // Try to find the section 'charset'

    for (; *ptr != '\0'; ptr++) {

        if (isspace(*ptr) || *ptr == ';') {
            continue;
        }

        if (curl_strnequal(ptr, "charset", 7)) {
            ptr += 7;
            break;
        }

        // Current section is not "charset", continue until the end of
        // the current section
        while (*ptr != '\0' && *ptr != ';') {
            ptr++;
        }

    }

    // Skip spaces after the section name 'charset'. If we are at the end
    // of the string, we just don't do anything.

    while (*ptr != '\0' && isspace(*ptr)) {
        ptr++;
    }

    // We expect '=' char here. Here we also check the end of the string.
    if (*ptr != '=') {
        goto skipCharset;
    }
    ptr++;

    // Skip spaces after char '='
    while (*ptr != '\0' && isspace(*ptr)) {
        ptr++;
    }

    // Check if value is quoted
    int is_quoted;
    if (*ptr == '"') {
        is_quoted = 1;
        ptr++;
    } else {
        is_quoted = 0;
    }

    // Save the value
    for (; *ptr != '\0'; ptr++) {

        // If we have quoted value, then stop on quote. Otherwise, stop on space or
        // the end of the current section (';')
        if (is_quoted) {
            if (*ptr == '"') {
                break;
            }
        } else {
            if (*ptr == ';' || isspace(*ptr)) {
                break;
            }
        }

        if (charset_len < sizeof(charset_buf)) {
            charset_buf[charset_len++] = tolower(*ptr);
        }

    }

skipCharset:

    *type_ptr = Tcl_NewStringObj(type_buf, type_len);
    *charset_ptr = Tcl_NewStringObj(charset_buf, charset_len);

    DBG2(printf("return: ok (type:[%s] charset:[%s])", Tcl_GetString(*type_ptr), Tcl_GetString(*charset_ptr)));

}
