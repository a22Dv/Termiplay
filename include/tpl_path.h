#ifndef TERMIPLAY_PATH
#define TERMIPLAY_PATH

#include <Windows.h>
#include <PathCch.h>
#include <Shlwapi.h>
#include "tpl_string.h"

typedef wstring win_path;

/// @brief Creates a path using a predefined string value, NULL if non-valid or if `path` is NULL.
/// @return A pointer to a heap-allocated wstring path.
static inline win_path* path_create(const wchar_t* path) {
    win_path* wp = wstr_create();
    if (path == NULL) {
        wstr_destroy(&wp);
        return NULL;
    }
    wstr_mulpush(wp, path);
    if (!path_is_syntactical(wp)) {
        wstr_destroy(&wp);
        return NULL;
    }
    return wp;
}

/// @brief Returns the path of the current executable. NULL upon failure. Supports up to `MAX_PATH`.
/// @return Path of the current executable.
static inline win_path* get_exec_path() {
    wchar_t exec_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exec_path, MAX_PATH);
    if (len == 0) {
        return NULL;
    }
    // Possible truncation.
    if (len == MAX_PATH) {
        return NULL;
    }
    return path_create(exec_path);
}

static inline win_path* get_parent(win_path* path) {
    
}
static inline win_path* join() {}
static inline bool path_is_syntactical(win_path* path) {}
static inline bool path_exists() {}
static inline win_path* path_resolve() {}
static inline void destroy_path() {}
#endif