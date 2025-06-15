#ifndef TERMIPLAY_STRING
#define TERMIPLAY_STRING

#include <tpl_vector.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/// @brief Is a variant of "vector" with specific function implementations.
/// Do NOT use `vec_{FUNC_NAME}` functions on this. Use `str_{FUNC_NAME}`.
typedef vector string;

/// @brief Is a variant of "vector" with specific function implementations.
/// Do NOT use `vec_{FUNC_NAME}` functions on this. Use `wstr_{FUNC_NAME}`
typedef vector wstring;

/// @brief Creates a string.
/// @return A pointer to a heap-allocated string.
static inline string* str_create() {
    string* str = vec_create(sizeof(char));
    if (str == NULL) {
        return NULL;
    }
    ((char*)str->data)[0] = '\0';
    return str;
}
/// @brief Creates a wide string.
/// @return A pointer to a heap-allocated wide string.
static inline wstring* wstr_create() {
    wstring* wstr = vec_create(sizeof(wchar_t));
    if (wstr == NULL) {
        return NULL;
    }
    ((wchar_t*)wstr->data)[0] = '\0';
    return wstr;
}

/// @brief  Ensures the capacity of a string is at least target + 1 (null-terminator)
/// @param str String
/// @param target Target capacity without null-termination.
/// @return Boolean signifying success or failure
static inline bool str_ensure_capacity(
    string* str,
    size_t target
) {
    return vec_ensure_capacity(str, target + 1);
}
/// @brief  Ensures the capacity of a wide string is at least target + 1 (null-terminator)
/// @param str Wide string
/// @param target Target capacity without null-termination.
/// @return Boolean signifying success or failure
static inline bool wstr_ensure_capacity(
    wstring* wstr,
    size_t target
) {
    return vec_ensure_capacity(wstr, target + 1);
}
/// @brief Pushes multiple `char`s onto the string.
/// @param str String
/// @param data char* to characters.
/// @return Boolean signifying success or failure.
static inline bool str_mulpush(
    string* str,
    const char* data
) {
    if (str == NULL || data == NULL) {
        return false;
    }
    const size_t data_count = strlen(data);

    // Cannot or can only fit enough chars with the null-terminator (-1)
    if (str->len + data_count > str->capacity - 1) {
        if (!str_ensure_capacity(str, str->len + data_count)) {
            return false;
        }
    }
    memcpy((char*)str->data + str->len, data, data_count);
    str->len = str->len + data_count;
    ((char*)str->data)[str->len] = '\0';
    return true;
}

/// @brief Pushes multiple `wchar_t`s to a wide string.
/// @param wstr Wide string destination.
/// @param data Data to be pushed.
/// @return A boolean signifying success or failure.
static inline bool wstr_mulpush(
    wstring* wstr,
    const wchar_t* data
) {
    if (wstr == NULL || data == NULL) {
        return false;
    }
    const size_t data_count = wcslen(data);
    if (wstr->len + data_count > wstr->capacity - 1) {
        if (!wstr_ensure_capacity(wstr, wstr->len + data_count)) {
            return false;
        }
    }
    memcpy((wchar_t*)wstr->data + wstr->len, data, data_count * sizeof(wchar_t));
    wstr->len = wstr->len + data_count;
    ((wchar_t*)wstr->data)[wstr->len] = '\0';
    return true;
}
/// @brief `free()` a string.
/// @param str_ptr String.
static inline void str_destroy(string** str_ptr) { vec_destroy(str_ptr); }

/// @brief `free()` a wide string.
/// @param wstr_ptr Wide string.
static inline void wstr_destroy(wstring** wstr_ptr) { vec_destroy(wstr_ptr); }
#endif