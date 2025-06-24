#define DEBUG
#define THREAD_COUNT 3

#include <Windows.h>
#include <stdio.h>
#include "tpl_app.h"
#include "tpl_errors.h"
#include "tpl_ffmpeg_utils.h"
#include "tpl_input.h"
#include "tpl_path.h"
#include "tpl_player.h"
#include "tpl_utils.h"

/// @brief Starts player execution.
/// @param video_path Path to video file.
/// @return Return code.
tpl_result start_execution(wpath* video_path) {

    // Set variables.
    tpl_result        return_code        = TPL_SUCCESS;
    wpath*            resolved_path      = NULL;
    wpath*            config_path        = NULL;
    tpl_player_conf*  pl_config          = NULL;
    tpl_player_state* pl_state           = NULL;
    volatile LONG     writer_pconf_flag  = false;
    volatile LONG     writer_pstate_flag = false;
    volatile LONG     shutdown           = false;
    tpl_thread_data*  audio_thread_data  = NULL;
    tpl_thread_data*  video_thread_data  = NULL;
    tpl_thread_data*  proc_thread_data   = NULL;
    HANDLE            audio_thread       = NULL;
    HANDLE            video_thread       = NULL;
    HANDLE            proc_thread        = NULL;
    uint8_t           key_code           = 0;
    const uint16_t    polling_rate_ms    = 50;
    const double seek_speed_table[10] = {
        -120.0, -60.0, -30.0, -10.0, -5.0, 5.0, 10.0, 30.0, 60.0, 120.0
    };

    // Resolving video path.
    tpl_result resolve_call = wpath_resolve_path(video_path, &resolved_path);
    if (tpl_failed(resolve_call)) {
        fwprintf(stderr, L"Path cannot be resolved.\n");
        IF_ERR_GOTO(true, resolve_call, return_code);
    }

    // Check for file existence.
    bool exists = wpath_exists(resolved_path);
    if (!exists) {
        fwprintf(stderr, L"File does not exist or cannot be found.\n");
        IF_ERR_GOTO(true, TPL_INVALID_PATH, return_code);
    }

    // Check for file validity.
    bool       valid_file = false;
    tpl_result valid_call = tpl_av_file_stream_valid(resolved_path, &valid_file);
    if (tpl_failed(valid_call)) {
        fwprintf(stderr, L"File is not in a supported format or is corrupted.\n");
        IF_ERR_GOTO(true, valid_call, return_code);
    }
    if (!valid_file) {
        fwprintf(stderr, L"File A/V streams cannot be found.\n");
        IF_ERR_GOTO(true, TPL_INVALID_DATA, return_code);
    }

    // Retrieving configuration file path.
    tpl_result conf_call = tpl_get_config_path(&config_path);
    IF_ERR_GOTO(tpl_failed(conf_call), conf_call, return_code);

    // Player state/configuration initialization.
    tpl_result init_pcall = tpl_player_init(resolved_path, config_path, &pl_config);
    IF_ERR_GOTO(tpl_failed(init_pcall), init_pcall, return_code);
    tpl_result set_conf_call = tpl_player_setconf(pl_config, &writer_pconf_flag);
    IF_ERR_GOTO(tpl_failed(set_conf_call), set_conf_call, return_code);
    tpl_result set_state_call = tpl_player_setstate(&pl_state);
    IF_ERR_GOTO(tpl_failed(set_state_call), set_state_call, return_code);

    // Create look-up tables.
    HANDLE* lookup_thread_handle_address[THREAD_COUNT] = {
        &audio_thread, &video_thread, &proc_thread
    };
    const int lookup_id[THREAD_COUNT] = {TPL_AUDIO_THREAD, TPL_VIDEO_THREAD, TPL_PROC_THREAD};
    tpl_thread_data** lookup_thread_data_address[THREAD_COUNT] = {
        &audio_thread_data, &video_thread_data, &proc_thread_data
    };

    // Create thread data, and start threads.
    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        tpl_result thread_data_create_call = tpl_thread_data_create(
            lookup_thread_data_address[i], lookup_id[i], pl_config, pl_state, &writer_pconf_flag,
            &shutdown, &writer_pstate_flag
        );
        IF_ERR_GOTO(tpl_failed(thread_data_create_call), thread_data_create_call, return_code);
        *(lookup_thread_handle_address[i]) = (HANDLE)_beginthreadex(
            NULL, 0, tpl_start_thread, (void*)(*(lookup_thread_data_address[i])), 0, NULL
        );
        IF_ERR_GOTO(
            *(lookup_thread_handle_address[i]) == NULL, TPL_THREAD_CREATION_FAILURE, return_code
        );
    }

    // Main loop.
    while (true) {

        // Poll for input.
        key_code             = tpl_poll_input();
        tpl_result proc_call = tpl_proc_input(
            key_code, polling_rate_ms, seek_speed_table, pl_state, pl_config, &shutdown, &writer_pconf_flag,
            &writer_pstate_flag
        );
        IF_ERR_GOTO(tpl_failed(proc_call), proc_call, return_code);

        // Shutdown.
        if (_InterlockedOr(&shutdown, 0)) {
            goto unwind;
        }

#ifdef DEBUG // Debug print.
        _wsystem(L"cls");
        wprintf(
            L"[STATE DEBUG]\n"
            L"[LOOPING] = %ls\n"
            L"[PLAYING] = %ls\n"
            L"[PRESET INDEX] = %i\n"
            L"[MUTED] = %ls\n"
            L"[SEEKING] = %ls\n"
            L"[VOLUME LEVEL] = %lf\n"
            L"[SEEK MULTIPLE INDEX] = %lf\n"
            L"[M_CLOCK (TIMESTAMP)] %lf\n", pl_state->looping ? L"TRUE" : L"FALSE",
            pl_state->playing ? L"TRUE" : L"FALSE", pl_state->preset_idx,
            pl_state->muted ? L"TRUE" : L"FALSE", pl_state->seeking ? L"TRUE" : L"FALSE",
            pl_state->vol_lvl, pl_state->seek_multiple_idx, pl_state->main_clock
        );
#endif

        // Check for premature exits.
        for (size_t i = 0; i < THREAD_COUNT; ++i) {
            tpl_win_u32 exit_code     = 0;
            HANDLE      thread_handle = *(lookup_thread_handle_address[i]);
            if (thread_handle == NULL) {
                continue;
            }
            if (WaitForSingleObject(thread_handle, 0) == WAIT_OBJECT_0) {
                GetExitCodeThread(thread_handle, &exit_code);
                IF_ERR_GOTO(tpl_failed(exit_code), exit_code, return_code);
            }
        }

        // Preparation for next iteration.
        Sleep(polling_rate_ms);
        key_code = 0;
    }

unwind:
    _InterlockedExchange(&shutdown, true);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        shutdown_thread(&lookup_thread_handle_address[i]);
        free(lookup_thread_data_address[i]);
    }
    wpath_destroy(&resolved_path);
    wpath_destroy(&config_path);
    if (pl_state) {
        free(pl_state);
        pl_state = NULL;
    }

    if (pl_config) {
        str_destroy(&pl_config->char_preset1);
        str_destroy(&pl_config->config_utf8path);
        free(pl_config);
        pl_config = NULL;
    }
    return return_code;
}

// TODO LIST
// Set pipeline up with FFMPEG.
// Set player to start
// Enter while loop for player.
// Check current console width

// Resize frame accordingly while maintaining aspect ratio
// Transform to ASCII to display
// Set color if rgb is turned on
// Play and sync while waiting for user input.
// If user input, handle accordingly
// Change character map, change grayscale/rgb, play/pause, fast-forward/backward by 10s
