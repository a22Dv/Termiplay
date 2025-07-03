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
#include "tl_audio.h"
#include "tl_errors.h"
#include "tl_proc.h"
#include "tl_types.h"
#include "tl_utils.h"

/// @brief Executes the actual application.
/// @param media_path Path to media file.
/// @return Application exit code.
tl_result app_exec(WCHAR *media_path);

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

/// @brief Executes video thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result video_thread_exec(thread_data *thdata);
#endif