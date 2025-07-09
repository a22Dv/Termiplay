#pragma once

#define TL_DEBUG // Changes application logging behavior.

#define ERR_LIST                                                                                   \
    X(TL_SUCCESS, "SUCCESS")                                                                       \
    X(TL_GEN_ERR, "GENERIC ERROR")                                                                 \
    X(TL_DEP_NOT_FOUND, "DEPENDENCY NOT FOUND")

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
    fprintf(stderr, "[DBG-LOG] %s:%s():%i | %s", __FILE__, __func__, __LINE__, err_str(err))
#else
/// NO-OP. TL_DEBUG is not defined.
#define LOG_ERR(err) ((void)0)
#endif
