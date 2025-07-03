#ifndef TL_UTILS
#define TL_UTILS
#include <Windows.h>
#include <PathCch.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "tl_errors.h"
#include "tl_types.h"

#define BUFFER_SIZE 1024
#define WCHAR_PATH_MAX 512
#define LOWEST_WTHREAD_ID 0
#define HIGHEST_WTHREAD_ID 3
/// @brief Checks if a file is valid and passes the full resolved path if so. NULL otherwise.
/// @param media_path Path to validate.
/// @param resolved_path NULL-ed pointer to hold the path.
/// @return Return code.
tl_result is_valid_file(
    const WCHAR *media_path,
    WCHAR      **resolved_path
);

/// @brief Passes the number of streams in a given media file.
/// @param media_path Full resolved path to media file.
/// @param streams Pointer to store result.
/// @return Return code.
/// @note `*streams` will hold the count of media streams in the top byte, with the audio streams in
/// the bottom byte. Only succeeds when media file has at least one video or audio stream, but not
/// more than 1 each.
tl_result get_stream_count(
    const WCHAR *media_path,
    uint8_t     *streams
);

/// @brief Passes the metadata of a given media file.
/// @param media_path Full resolved path to media file.
/// @param streams Stream count value.
/// @param mtptr Pointer to store result.
/// @return Return code.
tl_result get_metadata(
    WCHAR        *media_path,
    const uint8_t streams,
    metadata    **mtptr
);

/// @brief Passes an array of heap-allocated thread data structs ordered by each thread's ID.
/// @param streams Stream count.
/// @param abuf1 Audio buffer 1.
/// @param abuf2 Audio buffer 2.
/// @param vbuf1 Video buffer 1.
/// @param vbuf2 Video buffer 2.
/// @param pstate Player state.
/// @param mtdta Media metadata.
/// @param thptr Out-parameter to store result.
/// @return Return code.
tl_result create_thread_data(
    uint8_t        streams,
    int16_t       *abuf1,
    int16_t       *abuf2,
    char         **vbuf1,
    char         **vbuf2,
    player_state  *pstate,
    metadata      *mtdta,
    thread_data ***thptr
);

/// @brief Creates a heap-allocated player state.
/// @param pl_state_ptr Out-parameter to store result.
/// @return Return code.
tl_result create_player_state(player_state **pl_state_ptr);

/// @brief Creates a heap-allocated helper thread data.
/// @param thdta Thread data.
/// @param order_event HANDLE to signal to wake thread.
/// @param finish_event HANDLE to signal a worker is finished
/// @param wthread_id Worker thread ID.
/// @param wth_ptr Pointer to store heap-allocated result.
/// @return Return code.
tl_result create_wthread_data(
    thread_data   *thdta,
    HANDLE         order_event,
    HANDLE         finish_event,
    HANDLE         shutdown_event,
    uint8_t        wthread_id,
    wthread_data **wth_ptr
);
/// @brief Prints state to console.
/// @param pstate Player state.
void state_print(player_state *plstate);


/// @brief Dispatches threads via `_beginthreadex`.
/// @param data Thread data.
/// @return `unsigned int` (Required for _beginthreadex)
unsigned int __stdcall thread_dispatcher(void *data);

/// @brief Dispatches threads via `_beginthreadex` specifically for helper worker threads.
/// @param data Worker thread data.
/// @return `unsigned int` (Required for _beginthreadex)
unsigned int __stdcall helper_thread_dispatcher(void *data);

#endif