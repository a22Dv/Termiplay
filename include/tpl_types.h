#ifndef TERMIPLAY_TYPES
#define TERMIPLAY_TYPES

#define NULL_DEVICE "nul"

typedef enum {
    TPL_SUCCESS = 0,
    TPL_GENERIC_ERROR = 1,
    TPL_FAILED_TO_GET_PATH = 2,
    TPL_TRUNCATED_PATH = 3,
    TPL_INCOMPATIBLE_VEC = 4,
    TPL_INVALID_PATH = 5,
    TPL_RECEIVED_NULL = 6
} tpl_result;

#endif
