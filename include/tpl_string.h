#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <tpl_vector.h>

/// path functions in setup must use w_strings.
/// @brief Resizable wide-character string.
typedef struct {
    wchar_t* string;
    size_t len;
} w_string;

// @brief
typedef struct {
    char* string;
    size_t len;
} string;

static inline w_string* wstr_create() {
    w_string* wstr = (w_string*)malloc(sizeof(w_string));
    if (wstr == NULL) {
        return NULL;
    }
    return wstr;
}
static inline string* str_create() {

}
static inline bool wstr_mulpush() {

}
static inline bool str_push() {
}
static inline void wstr_destroy() {
}
static inline void str_destroy() {
}
static inline wchar_t* wstr_at(size_t idx) {
}
static inline char* str_at(size_t idx) {
}
static inline wchar_t* wstr_data() {
}
static inline char* str_data() {
}
