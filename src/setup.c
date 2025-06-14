#include <pathCch.h>
#include <stdlib.h>
#include <tpl_setup.h>
#include <windows.h>

/// @brief Ensures a given buffer has a string that can fit MAX_PATH.
/// Does not guarantee if it even is a valid path at ALL. Rely on fopen() for
/// that.
/// @param input_wchar_t_str_nul Null-terminated string in a vector container.
/// @param path_output_wchar_t Buffer for a wchar_t path.
/// @return Return code.
tpl_result create_path(
    vector* input_wchar_t,
    vector* path_output_wchar_t
) {}

tpl_result get_exec_path(vector* buf_wchar_t) {
    if (buf_wchar_t == NULL) {
        return TPL_RECEIVED_NULL;
    }
    wchar_t exec_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exec_path, MAX_PATH);
    if (len == 0) {
        return TPL_FAILED_TO_GET_PATH;
    }
    if (len == MAX_PATH) {
        return TPL_TRUNCATED_PATH;
    }
    if (buf_wchar_t->idx_size == sizeof(wchar_t)) {
        // Includes the null-terminator.
        vec_mulpush(buf_wchar_t, (void*)exec_path, (size_t)len + 1);
    } else {
        return TPL_INCOMPATIBLE_VEC;
    }
    return TPL_SUCCESS;
}

tpl_result get_parent_path(
    const vector* path_wchar_t,
    vector* parent_path_wchar_t
) {
    if (path_wchar_t == NULL || parent_path_wchar_t == NULL) {
        return TPL_RECEIVED_NULL;
    }
    if (path_wchar_t->idx_size != sizeof(wchar_t) ||
        parent_path_wchar_t->idx_size != sizeof(wchar_t)) {
        return TPL_INCOMPATIBLE_VEC;
    }
    vector* pp_buffer_wchar_t = vec_create(sizeof(wchar_t));
    vec_mulpush(pp_buffer_wchar_t, path_wchar_t->data, path_wchar_t->len);
    HRESULT hr = PathCchRemoveFileSpec((PWSTR)pp_buffer_wchar_t->data, pp_buffer_wchar_t->capacity);
    if (SUCCEEDED(hr)) {
        // As the buffer is modified in-place, we need a way to get the new
        // length.
        size_t new_len = wcslen((const wchar_t*)pp_buffer_wchar_t->data);
        vec_mulpush(parent_path_wchar_t, pp_buffer_wchar_t->data, new_len + 1);
        vec_destroy(pp_buffer_wchar_t);
        return TPL_SUCCESS;
    } else {
        vec_destroy(pp_buffer_wchar_t);
        return TPL_FAILED_TO_GET_PATH;
    }
}

tpl_result append_path(
    const vector* src_path_wchar_t,
    vector* appended_path_wchar_t,
    const vector* nul_term_path_to_append
) {
    if (src_path_wchar_t == NULL || appended_path_wchar_t == NULL ||
        nul_term_path_to_append == NULL) {
        return TPL_RECEIVED_NULL;
    }
    if (src_path_wchar_t->idx_size != sizeof(wchar_t) ||
        appended_path_wchar_t->idx_size != sizeof(wchar_t) ||
        nul_term_path_to_append->idx_size != sizeof(wchar_t)) {
        return TPL_INCOMPATIBLE_VEC;
    }
    vector* ap_buffer_wchar_t = vec_create(sizeof(wchar_t));
    vec_mulpush(ap_buffer_wchar_t, src_path_wchar_t->data, src_path_wchar_t->len);
    vec_ensure_capacity(ap_buffer_wchar_t, MAX_PATH);
    HRESULT hr = PathCchAppend(
        (PWSTR)ap_buffer_wchar_t->data, ap_buffer_wchar_t->capacity,
        (PCWSTR)nul_term_path_to_append->data
    );
    if (SUCCEEDED(hr)) {
        size_t new_len = wcslen((const wchar_t*)ap_buffer_wchar_t->data);
        // + 1 to include null termination.
        vec_mulpush(appended_path_wchar_t, ap_buffer_wchar_t->data, new_len + 1);
        vec_destroy(ap_buffer_wchar_t);
        return TPL_SUCCESS;
    } else {
        vec_destroy(ap_buffer_wchar_t);
        return TPL_FAILED_TO_GET_PATH;
    }
}