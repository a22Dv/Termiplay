#pragma once

#define TL_DEBUG // Changes application logging behavior.

#define ERR_LIST                                                                                   \
    X(TL_SUCCESS, "SUCCESS")                                                                       \
    X(TL_GEN_ERR, "GENERIC ERROR")                                                                 \
    X(TL_DEP_NOT_FOUND, "DEPENDENCY NOT FOUND")                                                    \
    X(TL_NULL_ARG, "INVALID NULL ARGUMENT")                                                        \
    X(TL_ALREADY_INITIALIZED, "ALREADY INITIALIZED. NON-NULL POINTER RECEIVED.")                   \
    X(TL_ALLOC_FAILURE, "MEMORY ALLOCATION FAILURE.")                                              \
    X(TL_PIPE_CREATION_FAILURE, "PIPE CREATION FAILURE")                                           \
    X(TL_PIPE_PROC_FAILURE, "PIPE PROCESS FAILURE")                                                \
    X(TL_FORMAT_FAILURE, "FORMAT FAILURE")                                                         \
    X(TL_PIPE_READ_FAILURE, "PIPE READ FAILURE")                                                   \
    X(TL_INVALID_ARG, "INVALID ARGUMENT")                                                          \
    X(TL_OS_ERR, "OS ERROR")                                                                       \
    X(TL_CONSOLE_ERR, "CONSOLE ERROR")                                                             \
    X(TL_INVALID_FILE, "INVALID FILE")                                                             \
    X(TL_INCOMPLETE_DATA, "INCOMPLETE DATA")                                                       \
    X(TL_MINIAUDIO_ERR, "MINIAUDIO ERROR")                                                         \
    X(TL_COMPRESS_ERR, "COMPRESSION ERROR")

/// @brief Custom return code/value.
typedef enum _tl_result {
#define X(err, str) err,
    ERR_LIST
#undef X
} tl_result;

/// @brief Returns an error string.
/// @param err_code Error code.
/// @return `const char*` that holds the string representation of the error code.
static const char *err_str(const tl_result err_code) {
    switch (err_code) {
#define X(err, str)                                                                                \
    case err:                                                                                      \
        return str;
        ERR_LIST
#undef X
    default:
        return "UNSUPPORTED ERR_CODE";
    }
}
#ifdef TL_DEBUG
/// Logs errors to stderr.
#define LOG_ERR(err)                                                                               \
    fprintf(stderr, "[DBG-LOG] %s:%s():%i | %s\n", __FILE__, __func__, __LINE__, err_str(err))
#else
/// NO-OP. TL_DEBUG is not defined.
#define LOG_ERR(err) ((void)0)
#endif
