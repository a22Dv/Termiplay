#ifndef TL_PROC
#define TL_PROC

#include <stdbool.h>
#include <stdio.h>
#include <process.h>
#include "Windows.h"
#include "tl_errors.h"
#include "tl_types.h"
#include "tl_utils.h"

#define WORKER_A1 0
#define WORKER_A2 1
#define WORKER_V1 2
#define WORKER_V2 3
#define COMMAND_BUFFER_SIZE 1024


/// @brief Executes producer thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result proc_thread_exec(thread_data *thdata);

/// @brief Writes to a given audio buffer.
/// @param serial_at_call Data serial at call.
/// @param pstate Player state.
/// @param clock_start Clock start of decoding.
/// @param media_path Path to media file.
/// @param streams Stream bitmask.
/// @param buffer Buffer to write to.
/// @return Return code.
tl_result write_abuffer(
    const size_t  serial_at_call,
    player_state *pstate,
    const double  clock_start,
    const WCHAR  *media_path,
    const uint8_t streams,
    int16_t      *buffer
);

/// @brief Writes to a given audio buffer.
/// @param serial_at_call Data serial at call.
/// @param pstate Player state.
/// @param clock_start Clock start of decoding.
/// @param media_path Path to media file.
/// @param streams Stream bitmask.
/// @param buffer Buffer to write to.
/// @param default_charset Whether to use the default character system or the other one.
/// @return Return code.
tl_result write_vbuffer_compressed(
    const size_t  serial_at_call,
    player_state *pstate,
    const double  clock_start,
    const WCHAR  *media_path,
    const uint8_t streams,
    char        **buffer,
    const bool    default_charset
);

/// @brief Executes the audio decoder helper thread.
/// @param worker_thdata Worker thread data.
/// @return Return code.
tl_result audio_helper_thread_exec(wthread_data *worker_thdata);

/// @brief Executes video decoder helper thread.
/// @param worker_thdata Worker thread data.
/// @return Return code.
tl_result video_helper_thread_exec(wthread_data *worker_thdata);

#endif