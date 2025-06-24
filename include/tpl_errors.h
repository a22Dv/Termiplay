#ifndef TERMIPLAY_ERRORS
#define TERMIPLAY_ERRORS

#include <stdbool.h>
#include <stdio.h>
#ifdef DEBUG
#define LOG_ERR(err)                                                                               \
    do {                                                                                           \
        fwprintf(                                                                                  \
            stderr, L"[ERROR] %ls:%i in %ls() | %ls\n", WIDE(__FILE__), __LINE__, __FUNCTIONW__,   \
            err_str(err)                                                                           \
        );                                                                                         \
    } while (0)
#else
#define LOG_ERR(err) ((void)0)
#endif
#define WIDE_H(x) L##x

// Expands x before ##
#define WIDE(x) WIDE_H(x)
#define IF_ERR_RET(condition, err_code)                                                            \
    do {                                                                                           \
        if (condition) {                                                                           \
            LOG_ERR(err_code);                                                                     \
            return err_code;                                                                       \
        }                                                                                          \
    } while (0)
#define IF_ERR_GOTO(condition, err, ret_var)                                                       \
    do {                                                                                           \
        if (condition) {                                                                           \
            LOG_ERR(err);                                                                          \
            ret_var = err;                                                                         \
            goto unwind;                                                                           \
        }                                                                                          \
    } while (0)

// X-macro, to sync both enums and the switch-cases in err_str().
#define RESULT_LIST                                                                                \
    X(TPL_SUCCESS, L"SUCCESSFUL OPERATION")                                                        \
    X(TPL_GENERIC_ERROR, L"GENERIC ERROR")                                                         \
    X(TPL_FAILED_TO_GET_PATH, L"FAILED TO GET PATH")                                               \
    X(TPL_TRUNCATED_PATH, L"PATH WAS TRUNCATED")                                                   \
    X(TPL_INCOMPATIBLE_VEC, L"INCOMPATIBLE VECTOR")                                                \
    X(TPL_INVALID_PATH, L"INVALID PATH")                                                           \
    X(TPL_RECEIVED_NULL, L"INVALID NULL ARGUMENT")                                                 \
    X(TPL_INVALID_ARGUMENT, L"INVALID ARGUMENT")                                                   \
    X(TPL_ALLOC_FAILED, L"MEMORY ALLOCATION FAILURE")                                              \
    X(TPL_INDEX_ERROR, L"INDEX OUT OF BOUNDS")                                                     \
    X(TPL_OVERFLOW, L"VALUE OVERFLOW")                                                             \
    X(TPL_OVERWRITE, L"INVALID NON-NULL POINTER. MEMORY LEAK/OVERWRITE POSSIBLE.")                 \
    X(TPL_FAILED_TO_RESOLVE_PATH, L"FAILED TO RESOLVE PATH")                                       \
    X(TPL_FAILED_TO_PARSE, L"FAILED TO PARSE")                                                     \
    X(TPL_FAILED_TO_PIPE, L"FAILED TO PIPE I/O")                                                   \
    X(TPL_UNSUPPORTED_STREAM, L"UNSUPPORTED STREAM FILE TYPE")                                     \
    X(TPL_DEPENDENCY_NOT_FOUND, L"DEPENDENCY NOT FOUND")                                           \
    X(TPL_FAILED_TO_OPEN_CONFIG, L"FAILED TO OPEN CONFIG")                                         \
    X(TPL_YAML_ERROR, L"YAML PARSING ERROR")                                                       \
    X(TPL_INVALID_CONFIG, L"INVALID CONFIG FILE")                                                  \
    X(TPL_FAILED_TO_GET_CONSOLE_DIMENSIONS, L"FAILED TO GET CONSOLE DIMENSTIONS")                  \
    X(TPL_FAILED_TO_INITIALIZE_AUDIO, L"FAILED TO INITIALIZE AUDIO")                               \
    X(TPL_FAILED_TO_START_AUDIO, L"FAILED TO START AUDIO")                                         \
    X(TPL_INVALID_DATA, L"RECEIVED INVALID DATA")                                                  \
    X(TPL_THREAD_CREATION_FAILURE, L"FAILED TO CREATE THREAD")                                     \
    X(TPL_FAILED_TO_CLOSE_THREAD, L"FAILURE TO CLOSE THREAD")                                      \
    X(TPL_PIPE_ERROR, L"PIPE ERROR")

typedef enum {
#define X(value, string) value,
    RESULT_LIST
#undef X
} tpl_result;

static inline const wchar_t* err_str(tpl_result status_code) {
    switch (status_code) {
#define X(value, string)                                                                           \
    case value:                                                                                    \
        return string;
        RESULT_LIST
#undef X
    default:
        return L"UNHANDLED ERROR CODE";
    }
}

/// @brief Simply `return_code != TPL_SUCCESS`.
static inline const bool tpl_failed(tpl_result return_code) { return return_code != TPL_SUCCESS; }
#endif