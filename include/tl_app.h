#ifndef TL_APP
#define TL_APP

#include <Windows.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "tl_errors.h"
#include "tl_types.h"
#include "tl_utils.h"

/// @brief Executes the actual application.
/// @param media_path Path to media file.
/// @return Return code.
tl_result app_exec(WCHAR *media_path);

/// @brief Executes producer thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result proc_thread_exec(thread_data* thdata);

/// @brief Executes audio thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result audio_thread_exec(thread_data* thdata);

/// @brief Executes video thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result video_thread_exec(thread_data* thdata);

/// @brief Dispatches threads via `_beginthreadex`.
/// @param data Thread data.
/// @return `unsigned int` (Required for _beginthreadex)
unsigned int __stdcall thread_dispatcher(void* data);

#endif