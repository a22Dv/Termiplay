#ifndef TERMIPLAY_PLAYER
#define TERMIPLAY_PLAYER

#include <windows.h>
#include <conio.h>
#include <process.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "tpl_errors.h"
#include "tpl_path.h"
#include "tpl_string.h"
#include "tpl_types.h"
#include "tpl_vector.h"

#define SETTING_KEYS 7

#define TPL_AUDIO_THREAD 1
#define TPL_VIDEO_THREAD 2
#define TPL_PROC_THREAD 3

typedef struct {
    string* char_preset1;
    char    char_preset2;
    bool    frame_skip;
    uint8_t frame_rate;
    bool    preserve_aspect;
    bool    rgb_out;
    bool    gray_scale;
    wpath*  video_fpath;
    string* config_utf8path;
    SRWLOCK srw_lock;
} tpl_player_conf;

typedef struct {
    bool    looping;
    bool    playing;
    uint8_t preset_idx;
    bool    muted;
    bool    seeking;
    double  main_clock;
    double  vol_lvl;
    double  seek_multiple_idx;
    SRWLOCK srw_lock;
} tpl_player_state;

typedef struct {
    tpl_player_conf*  p_conf;
    tpl_player_state* p_state;
    volatile LONG*    writer_pconf_flag_ptr;
    volatile LONG*    shutdown_ptr;
    volatile LONG*    writer_pstate_flag_ptr;
    uint8_t           thread_id;
} tpl_thread_data;


/// @brief Initializes player state from configuration file.
/// @param video_fpath Path to video file. Must be valid.
/// @param player_state Pointer to NULL-ed ptr to hold the defined state.
/// @return Return code.
tpl_result tpl_player_init(
    wpath*            video_fpath,
    wpath*            config_path,
    tpl_player_conf** player_conf
);

/// @note DUE FOR A REFACTOR. Table-approach. Somewhat urgent.
/// @brief Sets a given configuration.
/// @param pl_config Player config.
/// @param write_pending_flag Flag to toggle for multi-threaded environments.
/// @return Return code.
tpl_result tpl_player_setconf(
    tpl_player_conf* pl_config,
    volatile LONG*   write_pending_flag
);

tpl_result tpl_player_setstate(tpl_player_state** pl_state);

/// @brief Launches threads and starts playback with a given valid state.
/// @param pl_conf Player config. Must be initialized.
/// @return Return code.
tpl_result tpl_player_start(tpl_player_conf** pl_conf);

/// @brief Clears threads and does clean-up before `free()`-ing player state.
/// @param pl_conf Player config.
void tpl_player_destroy(tpl_player_conf** pl_conf);

/// @brief Dispatcher function to be passed to _beginthreadex().
/// @param data void* pointer to a tpl_thread_data struct.
unsigned __stdcall tpl_start_thread(void* data);

/// @brief Assembles thread data to a given pointer.
/// @param thread_data_ptr NULL-ed location to thread pointer to store results.
/// @param thread_id Unique thread ID.
/// @param conf Player configuration
/// @param state Player state.
/// @param pconf_wflag_ptr Pointer to  writer flag for player configuration.
/// @param shutdown_ptr Pointer to the shutdown flag.
/// @param pstate_wflag_ptr Pointer to writer flag for player state.
/// @return Return code.
tpl_result tpl_thread_data_create(
    tpl_thread_data** thread_data_ptr,
    uint8_t           thread_id,
    tpl_player_conf*  conf,
    tpl_player_state* state,
    volatile LONG*    pconf_wflag_ptr,
    volatile LONG*    shutdown_ptr,
    volatile LONG*    pstate_wflag_ptr
);
#endif
