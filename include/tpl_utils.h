#ifndef TERMIPLAY_UTILITIES
#define TERMIPLAY_UTILITIES
#include "tpl_errors.h"
#include "tpl_path.h"

/// @brief Gets the configuration file path.
/// @param conf_path_ptr Pointer to store the result.
/// @return Return code.
static tpl_result tpl_get_config_path(wpath** conf_path_ptr) {
    tpl_result return_code = TPL_SUCCESS;
    if (conf_path_ptr == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return_code = TPL_RECEIVED_NULL;
        goto cleanup;
    }
    if (*conf_path_ptr != NULL) {
        LOG_ERR(TPL_OVERWRITE);
        return_code = TPL_OVERWRITE;
        goto cleanup;
    }
    wpath*     exec_path      = NULL;
    tpl_result exec_path_call = wpath_get_exec_path(&exec_path);
    if (tpl_failed(exec_path_call)) {
        LOG_ERR(exec_path_call);
        return_code = exec_path_call;
        goto cleanup;
    }
    wpath*     exec_dir      = NULL;
    tpl_result exec_dir_call = wpath_get_parent_path(exec_path, &exec_dir);
    if (tpl_failed(exec_dir_call)) {
        LOG_ERR(exec_dir_call);
        return_code = exec_dir_call;
        goto cleanup;
    }
    wpath*     config_path_name = NULL;
    tpl_result init_conf_call   = wpath_init(&config_path_name, L"config.yaml");
    if (tpl_failed(init_conf_call)) {
        LOG_ERR(init_conf_call);
        return_code = init_conf_call;
        goto cleanup;
    }
    wpath*     config_path      = NULL;
    tpl_result config_path_call = wpath_join_path(exec_dir, config_path_name, &config_path);
    if (tpl_failed(config_path_call)) {
        LOG_ERR(config_path_call);
        return_code = config_path_call;
        goto cleanup;
    }
    *conf_path_ptr = config_path;
    config_path    = NULL;
cleanup:
    // destroy() calls are already no-ops if NULL.
    wpath_destroy(&exec_path);
    wpath_destroy(&exec_dir);
    wpath_destroy(&config_path_name);
    wpath_destroy(&config_path);
    return return_code;
}

static tpl_result tpl_get_console_dimensions(
    uint16_t* width,
    uint16_t* height
) {
    IF_ERR_RET(width == NULL, TPL_RECEIVED_NULL);
    IF_ERR_RET(height == NULL, TPL_RECEIVED_NULL);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        LOG_ERR(TPL_FAILED_TO_GET_CONSOLE_DIMENSIONS);
        return TPL_FAILED_TO_GET_CONSOLE_DIMENSIONS;
    }
    *height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    *width  = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    return TPL_SUCCESS;
}
#endif