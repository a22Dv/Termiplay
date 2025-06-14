#ifndef TERMIPLAY_SETUP
#define TERMIPLAY_SETUP
#include <tpl_types.h>
#include <tpl_vector.h>

/// @brief Fills a `wchar_t` buffer with the path of the current executable
/// including the null-terminator.
/// @param buf_wchar_t Destination vector. Assumed vec->idx_size ==
/// sizeof(wchar_t)
/// @return Return code.
tpl_result get_exec_path(vector* path_buffer_ch);

/// @brief Gives the parent path of a given path to a passed buffer.
/// @param path_wchar_t Path. Assumed to be ->idx_size == sizeof(wchar_t).
/// @param parent_path_wchar_t Buffer that holds the parent path.
/// @return Return code.
tpl_result get_parent_path(
    const vector* path_wchar_t,
    vector* parent_path_wchar_t
);
tpl_result append_path(
    const vector* src_path_wchar_t,
    vector* appended_path_wchar_t,
    const vector* nul_term_path_to_append
);

#endif