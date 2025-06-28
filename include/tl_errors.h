#ifndef TL_ERRORS
#define TL_ERRORS
#define DEBUG

// A single macro "list" to keep track of error codes.
#define ERR_LIST                                                                                   \
    X(TL_SUCCESS, "SUCCESSFUL OPERATION")                                                          \
    X(TL_GENERIC_ERROR, "GENERIC ERROR")

/// @brief Return/Exit code.
typedef enum tl_result {
#define X(err, str_err) err,
    ERR_LIST
#undef X
} tl_result;

#ifdef DEBUG
// Logs any exit code received as a formatted string.
#define LOG_ERR(exit_code)                                                                         \
    fprintf(stderr, "[DBG-ERR] %s:%i %s() | %s\n", __FILE__, __LINE__, __func__, err_str(exit_code))
#else
// DEBUG is not defined.
#define LOG_ERR(exit_code) ((void)0)
#endif

// Tries an expression, does the given action upon failure. Function must return `tl_result`.
#define TRY(exitc_var, func_call, action)                                                          \
    do {                                                                                           \
        tl_result _tryexit = func_call;                                                            \
        if (_tryexit != TL_SUCCESS) {                                                              \
            LOG_ERR(_tryexit);                                                                     \
            exitc_var = _tryexit;                                                                  \
            action;                                                                                \
        }                                                                                          \
    } while (0)

// Condition will be evaluated, logs the error, sets `exitc_var` and does the given action upon
// failure.
#define CHECK(exitc_var, failure_condition, err, action)                                           \
    do {                                                                                           \
        if (failure_condition) {                                                                   \
            LOG_ERR(err);                                                                          \
            exitc_var = err;                                                                       \
            action;                                                                                \
        }                                                                                          \
    } while (0)

/// @brief Returns the string representation of an error.
/// @param err Return/Exit code.
/// @return String representation of the error.
const char* err_str(tl_result err) {
    switch (err) {
#define X(err, str_err)                                                                            \
    case err:                                                                                      \
        return str_err;
        ERR_LIST
#undef X
    default:
        return "UNHANDLED EXIT CODE";
    }
}
#endif
