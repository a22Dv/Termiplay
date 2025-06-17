#ifndef TERMIPLAY_STRING
#define TERMIPLAY_STRING

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "tpl_types.h"
#include "tpl_vector.h"

typedef struct {
    vec* buffer;
} string;

typedef struct {
    vec* buffer;
} wstring;

/// @brief Creates a string.
/// @return A pointer to a heap-allocated string.
static inline tpl_result str_create(string** buffer) {
    tpl_result return_code = TPL_SUCCESS;
    string* str            = NULL;
    vec* v                 = NULL;
    if (buffer == NULL) {
        return_code = TPL_RECEIVED_NULL;
        LOG_ERR(return_code);
        goto error;
    }
    if (*buffer != NULL) {
        return_code = TPL_OVERWRITE;
        LOG_ERR(return_code);
        goto error;
    }

    str = malloc(sizeof(string));
    if (str == NULL) {
        return_code = TPL_ALLOC_FAILED;
        LOG_ERR(return_code);
        goto error;
    }
    tpl_result init_result = vec_init(sizeof(char), &v);
    if (init_result != TPL_SUCCESS) {
        return_code = init_result;
        LOG_ERR(return_code);
        goto error;
    }
    ((char*)v->data)[0] = '\0';
error:
    if (return_code != TPL_SUCCESS) {
        if (str) {
            free(str);
        }
        if (v) {
            vec_destroy(&v);
        }
        return return_code;
    }
    str->buffer = v;
    *buffer     = str;
    return return_code;
}
/// @brief Creates a wide string.
/// @return A pointer to a heap-allocated wide string.
static inline tpl_result wstr_create(wstring** buffer) {
    tpl_result return_code = TPL_SUCCESS;
    if (buffer == NULL) {
        return_code = TPL_RECEIVED_NULL;
        LOG_ERR(return_code);
        goto error;
    }
    if (*buffer != NULL) {
        return_code = TPL_OVERWRITE;
        LOG_ERR(return_code);
        goto error;
    }
    wstring* wstr = NULL;
    vec* v        = NULL;
    wstr          = malloc(sizeof(wstring));
    if (wstr == NULL) {
        return_code = TPL_ALLOC_FAILED;
        LOG_ERR(return_code);
        goto error;
    }
    tpl_result init_result = vec_init(sizeof(tpl_wchar), &v);
    if (init_result != TPL_SUCCESS) {
        return_code = init_result;
        LOG_ERR(return_code);
        goto error;
    }
    ((tpl_wchar*)v->data)[0] = L'\0';
error:
    if (return_code != TPL_SUCCESS) {
        if (wstr) {
            free(wstr);
        }
        if (v) {
            vec_destroy(&v);
        }
        return return_code;
    }
    wstr->buffer = v;
    *buffer      = wstr;
    return return_code;
}

/// @brief  Ensures the capacity of a string is at least target + 1 (null-terminator)
/// @param str String
/// @param target Target capacity without null-termination.
/// @return Boolean signifying success or failure
static inline tpl_result str_ensure_capacity(
    string* str,
    const size_t target
) {
    return vec_ensure_capacity(str->buffer, target + 1);
}
/// @brief  Ensures the capacity of a wide string is at least target + 1 (null-terminator)
/// @param str Wide string
/// @param target Target capacity without null-termination.
/// @return Return status
static inline tpl_result wstr_ensure_capacity(
    wstring* wstr,
    const size_t target
) {
    return vec_ensure_capacity(wstr->buffer, target + 1);
}
/// @brief Pushes multiple `char`s onto the string. Relies on null-terminated strings.
/// @param str String
/// @param data char* to characters.
/// @return Return status
static inline tpl_result str_mulpush(
    string* str,
    const char* data
) {
    if (str == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    // Put on a separate line instead of "||"-ed for debugging purposes.
    if (data == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    const size_t data_count  = strlen(data);
    tpl_result ensure_result = TPL_SUCCESS;
    if (SIZE_MAX - str->buffer->len < data_count + 1) {
        LOG_ERR(TPL_OVERFLOW);
        return TPL_OVERFLOW;
    }
    if (str->buffer->capacity < str->buffer->len + data_count + 1) {
        ensure_result = str_ensure_capacity(str, str->buffer->len + data_count);
    }
    if (ensure_result != TPL_SUCCESS) {
        LOG_ERR(ensure_result);
        return ensure_result;
    }
    char* end = (char*)str->buffer->data + str->buffer->len;
    memcpy(end, data, data_count);
    str->buffer->len                             = str->buffer->len + data_count;
    ((char*)str->buffer->data)[str->buffer->len] = '\0';
    return TPL_SUCCESS;
}

/// @brief Pushes multiple `tpl_wchar`s to a wide string.
/// @param wstr Wide string destination.
/// @param data Data to be pushed.
/// @return Return status
static inline tpl_result wstr_mulpush(
    wstring* wstr,
    const tpl_wchar* data
) {
    if (wstr == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    // Put on a separate line instead of "||"-ed for debugging purposes.
    if (data == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    const size_t data_count  = wcslen(data);
    tpl_result ensure_result = TPL_SUCCESS;
    if (SIZE_MAX - wstr->buffer->len < data_count + 1) {
        LOG_ERR(TPL_OVERFLOW);
        return TPL_OVERFLOW;
    }
    if (wstr->buffer->capacity < wstr->buffer->len + data_count + 1) {
        ensure_result = wstr_ensure_capacity(wstr, wstr->buffer->len + data_count);
    }
    if (ensure_result != TPL_SUCCESS) {
        LOG_ERR(ensure_result);
        return ensure_result;
    }
    tpl_wchar* end = (tpl_wchar*)wstr->buffer->data + wstr->buffer->len;
    memcpy(end, data, data_count * sizeof(tpl_wchar));
    wstr->buffer->len                                   = wstr->buffer->len + data_count;
    ((tpl_wchar*)wstr->buffer->data)[wstr->buffer->len] = L'\0';
    return TPL_SUCCESS;
}

/// @brief Returns the internal tpl_wchar* array.
/// @param wstr Wide string.
/// @return Pointer to wstr's internal buffer. else NULL.
static inline tpl_wchar* wstr_c(wstring* wstr) {
    if (wstr == NULL) {
        return NULL;
    }
    return (tpl_wchar*)wstr->buffer->data;
}

/// @brief Returns the internal char* array.
/// @param str Wide string.
/// @return Pointer to str's internal buffer. else NULL.
static inline char* str_c(string* str) {
    if (str == NULL) {
        return NULL;
    }
    return (char*)str->buffer->data;
}

/// @brief Returns the internal tpl_wchar* array.
/// @param wstr Wide string.
/// @return Pointer to wstr's internal buffer. else NULL.
static inline const tpl_wchar* wstr_c_const(wstring* wstr) {
    if (wstr == NULL) {
        return NULL;
    }
    return (const tpl_wchar*)wstr->buffer->data;
}

/// @brief Returns the internal char* array.
/// @param str Wide string.
/// @return Pointer to str's internal buffer. else NULL.
static inline const char* str_c_const(string* str) {
    if (str == NULL) {
        return NULL;
    }
    return (const char*)str->buffer->data;
}

/// @brief `free()` a string.
/// @param str_ptr String.
static inline void str_destroy(string** str_ptr) {
    if (str_ptr == NULL) {
        return;
    }
    vec_destroy(&((*str_ptr)->buffer));
    free(*str_ptr);
    *str_ptr = NULL;
}

/// @brief `free()` a wide string.
/// @param wstr_ptr Wide string.
static inline void wstr_destroy(wstring** wstr_ptr) {
    if (wstr_ptr == NULL) {
        return;
    }
    vec_destroy(&((*wstr_ptr)->buffer));
    free(*wstr_ptr);
    *wstr_ptr = NULL;
}
#endif