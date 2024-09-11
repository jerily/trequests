/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "common.h"
// for isdigit() / isspace()
#include <ctype.h>

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
