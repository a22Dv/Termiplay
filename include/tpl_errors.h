#ifndef TERMIPLAY_ERRORS
#define TERMIPLAY_ERRORS

#include <stdbool.h>
#include <stdio.h>
#ifdef DEBUG
#define LOG_ERR(err)                                                                               \
    do {                                                                                           \
        fprintf(                                                                                   \
            stderr, "[ERROR] %s:%i in %s() | %s\n", __FILE__, __LINE__, __func__, err_str(err)     \
        );                                                                                         \
    } while (0)
#else
#define LOG_ERR(err) ((void)0)
#endif

// X-macro, to sync both enums and the switch-cases in err_str().
#define RESULT_LIST                                                                                \
    X(TPL_SUCCESS, "SUCCESSFUL OPERATION")                                                         \
    X(TPL_GENERIC_ERROR, "GENERIC ERROR")                                                          \
    X(TPL_FAILED_TO_GET_PATH, "FAILED TO GET PATH")                                                \
    X(TPL_TRUNCATED_PATH, "PATH WAS TRUNCATED")                                                    \
    X(TPL_INCOMPATIBLE_VEC, "INCOMPATIBLE VECTOR")                                                 \
    X(TPL_INVALID_PATH, "INVALID PATH")                                                            \
    X(TPL_RECEIVED_NULL, "INVALID NULL ARGUMENT")                                                  \
    X(TPL_INVALID_ARGUMENT, "INVALID ARGUMENT")                                                    \
    X(TPL_ALLOC_FAILED, "MEMORY ALLOCATION FAILURE")                                               \
    X(TPL_INDEX_ERROR, "INDEX OUT OF BOUNDS")                                                      \
    X(TPL_OVERFLOW, "VALUE OVERFLOW")                                                              \
    X(TPL_OVERWRITE, "INVALID NON-NULL POINTER. MEMORY LEAK/OVERWRITE POSSIBLE.")                  \
    X(TPL_FAILED_TO_RESOLVE_PATH, "FAILED TO RESOLVE PATH")

typedef enum {
#define X(value, string) value,
    RESULT_LIST
#undef X
} tpl_result;

static inline const char* err_str(tpl_result status_code) {
    switch (status_code) {
#define X(value, string)                                                                           \
    case value:                                                                                    \
        return string;
        RESULT_LIST
#undef X
    default:
        return "UNHANDLED ERROR CODE";
    }
}


/// @brief Simply `return_code != TPL_SUCCESS`.
static inline const bool tpl_failed(tpl_result return_code) { return return_code != TPL_SUCCESS; }
#endif