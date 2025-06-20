#define DEBUG

#include <Windows.h>
#include <stdio.h>
#include "tpl_app.h"
#include "tpl_audio.h"
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
    // Check path validity.
    wpath*     resolved_path = NULL;
    tpl_result resolve_call  = wpath_resolve_path(video_path, &resolved_path);
    if (tpl_failed(resolve_call)) {
        fprintf(stderr, "Path cannot be resolved.\n");
        LOG_ERR(resolve_call);
        return resolve_call;
    }
    bool exists = wpath_exists(resolved_path);
    if (!exists) {
        fprintf(stderr, "File does not exist or cannot be found.\n");
        LOG_ERR(TPL_INVALID_ARGUMENT);
        return TPL_INVALID_ARGUMENT;
    }
    // Check for valid A/V streams in file.
    bool       valid_file = false;
    tpl_result valid_call = tpl_av_file_stream_valid(resolved_path, &valid_file);
    if (tpl_failed(valid_call)) {
        fprintf(stderr, "File is not in a supported format or is corrupted.\n");
        LOG_ERR(valid_call);
        return valid_call;
    }
    if (!valid_file) {
        fprintf(stderr, "File A/V streams cannot be found.\n");
        LOG_ERR(TPL_GENERIC_ERROR);
        return TPL_GENERIC_ERROR;
    }

    // Retrieving configuration file path.
    wpath*     config_path = NULL;
    tpl_result conf_call   = tpl_get_config_path(&config_path);
    if (tpl_failed(conf_call)) {
        LOG_ERR(conf_call);
        return conf_call;
    }

    // Player initialization.
    tpl_player_state* temp_pl_state   = NULL;
    tpl_player_conf*  temp_pl_config  = NULL;
    volatile LONG     temp_conf_flag  = false;
    const uint16_t    polling_rate_ms = 50;
    // 20 seek speed levels.
    const int16_t seek_multiple_table[20] = {-900, -600, -480, -360, -300, -240, -120,
                                             -60,  -30,  -10,  10,   30,   60,   120,
                                             240,  300,  360,  480,  600,  900};

    tpl_result init_pcall = tpl_player_init(resolved_path, config_path, &temp_pl_config);
    if (tpl_failed(init_pcall)) {
        LOG_ERR(init_pcall);
        return init_pcall;
    }
    tpl_result set_conf_call = tpl_player_setconf(temp_pl_config, &temp_conf_flag);
    if (tpl_failed(set_conf_call)) {
        LOG_ERR(set_conf_call);
        return set_conf_call;
    }
    tpl_result set_state_call = tpl_player_setstate(&temp_pl_state);
    if (tpl_failed(set_state_call)) {
        LOG_ERR(set_state_call);
        return set_state_call;
    }

    // Player state and flags.
    tpl_player_conf* pl_config       = temp_pl_config;
    temp_pl_config                   = NULL; // Moved pointer.
    tpl_player_state* pl_state       = temp_pl_state;
    temp_pl_state                    = NULL; // Moved pointer.
    volatile LONG writer_pconf_flag  = false;
    volatile LONG shutdown           = false;
    volatile LONG writer_pstate_flag = false;
    uint8_t       key_code           = 0;


    // Main loop.
    while (true) {
        key_code             = tpl_poll_input();
        tpl_result proc_call = tpl_proc_input(
            key_code, polling_rate_ms, pl_state, pl_config, &shutdown, &writer_pconf_flag,
            &writer_pstate_flag
        );

        // Debug print.
        wprintf(
            L"DBG: "
            L"CONF[cp1:'%hs'|cp2:%c|fs:%s|fr:%u|pa:%s|rgb:%s|gs:%s|slck:%p]\n"
            L"STATE[loop:%s|pts:%.2f|mut:%s|play:%s|pi:%d|smi:%lf|seek:%s|vol:%.2f|slck:%p]\n"
            L"FLAGS[pconf:%ld|shut:%ld|pstate:%ld] KEY[%u]\n",
            // Arguments for CONF
            str_c(pl_config->char_preset1), pl_config->char_preset2,
            pl_config->frame_skip ? L"T" : L"F", pl_config->frame_rate,
            pl_config->preserve_aspect ? L"T" : L"F", pl_config->rgb_out ? L"T" : L"F",
            pl_config->gray_scale ? L"T" : L"F", &pl_config->srw_lock,
            // Arguments for STATE
            pl_state->looping ? L"T" : L"F", pl_state->main_clock, pl_state->muted ? L"T" : L"F",
            pl_state->playing ? L"T" : L"F", pl_state->preset_idx, pl_state->seek_multiple_idx,
            pl_state->seeking ? L"T" : L"F", pl_state->vol_lvl, pl_state->srw_lock,
            // Arguments for FLAGS
            writer_pconf_flag, shutdown, writer_pstate_flag,
            // Argument for KEY
            key_code
        );
        if (_InterlockedOr(&shutdown, 0)) {
            break;
        }
        Sleep(polling_rate_ms);
        system("cls"); // debug
        key_code = 0;
    }
    return TPL_SUCCESS;

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
}
