#ifndef TERMIPLAY_PATH
#define TERMIPLAY_PATH

#include <Windows.h>
#include <PathCch.h>
#include <Shlwapi.h>
#include <stdbool.h>
#include "tpl_errors.h"
#include "tpl_string.h"
#include "tpl_types.h"

typedef wstring wpath;

static void wpath_destroy(wpath** path) { wstr_destroy(path); }

/// @brief Enforces MAX_PATH length limit to created paths. Recommended over wstr_init().
/// @param path Pointer to NULL-ed pointer that will hold the stored path.
/// @param path_data Optional initial path. Must be NULL-terminated. Pass NULL to ignore.
/// @return Return code.
static tpl_result wpath_init(
    wpath** path,
    const tpl_wchar* path_data
) {
    // wstr_init capable of handling NULL arguments.
    const tpl_result create_result = wstr_init(path);
    if (tpl_failed(create_result)) {
        LOG_ERR(create_result);
        return create_result;
    }
    // Early exit, no initialization required.
    if (path_data == NULL) {
        return TPL_SUCCESS;
    }
    const size_t path_len = wcslen(path_data);
    if (path_len >= MAX_PATH) {
        wpath_destroy(path);
        LOG_ERR(TPL_INVALID_PATH);
        return TPL_INVALID_PATH;
    }
    const tpl_result push_result = wstr_mulpush(*path, path_data);
    if (tpl_failed(push_result)) {
        wpath_destroy(path);
        LOG_ERR(push_result);
        return push_result;
    }
    return TPL_SUCCESS;
}

/// @brief Gets the current executable's path. Hardcoded to only support MAX_PATH length paths.
/// @param buffer Address of NULL-ed pointer to store the path.
/// @return Return code.
static tpl_result wpath_get_exec_path(wpath** buffer) {
    if (buffer == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (*buffer != NULL) {
        LOG_ERR(TPL_OVERWRITE);
        return TPL_OVERWRITE;
    }
    tpl_wchar buf[MAX_PATH];
    tpl_win_u32 len = GetModuleFileNameW(NULL, buf, MAX_PATH);
    if (len == 0) {
        LOG_ERR(TPL_FAILED_TO_GET_PATH);
        return TPL_FAILED_TO_GET_PATH;
    }
    if (len >= MAX_PATH) {
        LOG_ERR(TPL_TRUNCATED_PATH);
        return TPL_TRUNCATED_PATH;
    }
    tpl_result create_result = wpath_init(buffer, buf);
    if (tpl_failed(create_result)) {
        LOG_ERR(create_result);
        return create_result;
    }
    return TPL_SUCCESS;
}

/// @brief Checks if a path exists. Passing NULL is an automatic false.
/// @param path Path to check.
/// @return Boolean, whether a path exists or not.
static bool wpath_exists(wpath* path) {
    if (path == NULL) {
        return false;
    }
    bool result = PathFileExistsW(wstr_c(path));
    return result;
}

/// @brief Resolves a given path.
/// @param path Path to resolve.
/// @return Return code.
static tpl_result wpath_resolve_path(
    const wpath* path,
    wpath** buffer
) {
    if (path == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (buffer == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (*buffer != NULL) {
        LOG_ERR(TPL_OVERWRITE);
        return TPL_OVERWRITE;
    }
    tpl_wchar buf[MAX_PATH];
    tpl_win_u32 len = GetFullPathNameW(wstr_c_const(path), MAX_PATH, buf, NULL);
    if (len == 0) {
        LOG_ERR(TPL_FAILED_TO_RESOLVE_PATH);
        return TPL_FAILED_TO_RESOLVE_PATH;
    }
    if (len >= MAX_PATH) {
        LOG_ERR(TPL_TRUNCATED_PATH);
        return TPL_TRUNCATED_PATH;
    }
    tpl_result create_result = wpath_init(buffer, buf);
    if (tpl_failed(create_result)) {
        LOG_ERR(create_result);
        return create_result;
    }
    return TPL_SUCCESS;
}

/// @brief Joins two paths.
/// @param path_src Starting path.
/// @param path_more Path to be joined to starting path.
/// @param buffer Pointer to NULL-ed pointer to store the resulting path.
/// @return Return code.
static tpl_result wpath_join_path(
    const wpath* path_src,
    const wpath* path_more,
    wpath** buffer
) {
    // Separated checks instead of '||'-ing for ease in debugging.
    if (path_src == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (path_more == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (buffer == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (*buffer != NULL) {
        LOG_ERR(TPL_OVERWRITE);
        return TPL_OVERWRITE;
    }
    tpl_wchar buf[MAX_PATH];
    tpl_win_result join_result =
        PathCchCombine(buf, MAX_PATH, wstr_c_const(path_src), wstr_c_const(path_more));
    if (FAILED(join_result)) {
        LOG_ERR(TPL_FAILED_TO_GET_PATH);
        return TPL_FAILED_TO_GET_PATH;
    }
    tpl_result create_result = wpath_init(buffer, buf);
    if (tpl_failed(create_result)) {
        LOG_ERR(create_result);
        return create_result;
    }
    return TPL_SUCCESS;
}

/// @brief Returns the path one level above the given path.
/// @param path Given path.
/// @param buffer Pointer to NULL-ed pointer to store the result.
/// @return Return code.
static tpl_result wpath_get_parent_path(
    const wpath* path,
    wpath** buffer
) {
    if (path == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (buffer == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (*buffer != NULL) {
        LOG_ERR(TPL_OVERWRITE);
        return TPL_OVERWRITE;
    }
    tpl_wchar buf[MAX_PATH];
    errno_t cpy_res = wcscpy_s(buf, MAX_PATH, wstr_c_const(path));
    if (cpy_res != 0) {
        LOG_ERR(TPL_INVALID_PATH);
        return TPL_INVALID_PATH;
    }
    tpl_win_result par_result = PathCchRemoveFileSpec(buf, MAX_PATH);
    if (FAILED(par_result)) {
        LOG_ERR(TPL_FAILED_TO_GET_PATH);
        return TPL_FAILED_TO_GET_PATH;
    }
    tpl_result create_result = wpath_init(buffer, buf);
    if (tpl_failed(create_result)) {
        LOG_ERR(create_result);
        return create_result;
    }
    return TPL_SUCCESS;
}

/// @todo

/// @brief Converts a wpath in UTF-16 to a UTF-8 Variable-length encoded path.
/// @param input_path Input path in wchar_t*.
/// @param output_buffer Pointer to NULL-ed pointer to hold the output.
/// @return Return code.



#endif
