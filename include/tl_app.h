#ifndef TL_APP
#define TL_APP

#include <Windows.h>
#include <conio.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "miniaudio.h"
#include "tl_errors.h"
#include "tl_types.h"
#include "tl_utils.h"
#include "tl_proc.h"

#define MINIAUDIO_IMPLEMENTATION

#define LOOP 'l'
#define MUTE 'm'
#define TOGGLE_CHARSET 'c'
#define TOGGLE_PLAYBACK ' '
#define QUIT 'q'
#define EXTENDED 0xE0
#define ARROW_UP 72
#define ARROW_DOWN 80
#define ARROW_LEFT 75
#define ARROW_RIGHT 77

#define POLLING_RATE_MS 50
#define SEEK_SPEEDS 8
#define MAX_BUFFER_COUNT 4

/// @brief Executes the actual application.
/// @param media_path Path to media file.
/// @return Application exit code.
tl_result app_exec(WCHAR *media_path);

/// @brief Executes producer thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result proc_thread_exec(thread_data *thdata);

/// @brief Executes audio thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result audio_thread_exec(thread_data *thdata);

/// @brief Executes video thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result video_thread_exec(thread_data *thdata);

/// @brief Polls for input and passes the key code.
/// @param key_code Stores result.
/// @return Return code.
/// @note Discards additional keystrokes in buffer. Only takes the starting key. Does not
/// distinguish between extended keys and regular keys, will send only the second byte for extended
/// keys.
tl_result poll_input(uint8_t *key_code);

/// @brief Alters player state based on given key code.
/// @param key_code Key code.
/// @param seek_multiples Seek speed multiples.
/// @param mtdta Video metadata.
/// @param plstate Player state.
/// @return Return code.
tl_result proc_input(
    const uint8_t   key_code,
    const uint16_t *seek_multiples,
    const metadata *mtdta,
    player_state   *plstate
);

/// @brief Dispatches threads via `_beginthreadex`.
/// @param data Thread data.
/// @return `unsigned int` (Required for _beginthreadex)
unsigned int __stdcall thread_dispatcher(void *data);

/// @brief Dispatches threads via `_beginthreadex` specifically for helper worker threads.
/// @param data Worker thread data.
/// @return `unsigned int` (Required for _beginthreadex)
unsigned int __stdcall helper_thread_dispatcher(void *data);

#endif