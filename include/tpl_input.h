#ifndef TERMIPLAY_INPUT
#define TERMIPLAY_INPUT

#include <conio.h>
#include <stdint.h>
#include "tpl_errors.h"
#include "tpl_player.h"
#include "tpl_string.h"

#define EXT_KEY_PREF 224
#define ARR_UP 72
#define ARR_DOWN 80
#define ARR_LEFT 75
#define ARR_RIGHT 77
#define LOOP 'l'
#define CHAR_PRST 'c'
#define RELOAD 'r'
#define PLAYBACK ' '
#define MUTE 'm'
#define SHUTDOWN 'q'
#define VOL_UP ARR_UP
#define VOL_DOWN ARR_DOWN
#define F_FORWARD ARR_RIGHT
#define F_BACKWARD ARR_LEFT
#define CHAR_PRESET_COUNT 3
#define SEEK_SPEED_MAX 10
#define VOL_INCR 0.05

static void discard_buffered_input() {
    while (_kbhit()) {
        _getch();
    }
}

/// @brief Polls input and modifies a given state depending on the flags.
/// @param state Player state to modify.
/// @return Return code after exiting.
static uint8_t tpl_poll_input() {
    if (!_kbhit()) {
        return 0;
    }
    uint8_t ch = _getch();
    if (ch == EXT_KEY_PREF) {
        int ch_sf = _getch();
        if (ARR_UP != ch_sf && ARR_DOWN != ch_sf && ARR_LEFT != ch_sf && ARR_RIGHT != ch_sf) {
            return 0;
        }
        discard_buffered_input();
        return ch_sf;
    }
    ch |= 0b00100000;
    if (('a' > ch || ch > 'z') && ch != ' ') {
        discard_buffered_input();
        return 0;
    }
    discard_buffered_input();
    return ch;
}

static tpl_result tpl_proc_input(
    const uint8_t     key_code,
    const uint8_t     polling_rate_ms,
    const double*     seek_speed_table,
    tpl_player_state* pl_state,
    tpl_player_conf*  pl_conf,
    volatile LONG*    shutdown,
    volatile LONG*    writer_pconf_flag,
    volatile LONG*    writer_pstate_flag
) {
    const double increment_magn = 0.5 * (double)polling_rate_ms / 1000.0;
    if (key_code == RELOAD) {
        tpl_result setconf_call = tpl_player_setconf(pl_conf, writer_pconf_flag);
        if (tpl_failed(setconf_call)) {
            LOG_ERR(setconf_call);
            return setconf_call;
        }
        return TPL_SUCCESS;
    } else if (key_code == SHUTDOWN) {
        _InterlockedExchange(shutdown, true);
        return TPL_SUCCESS;
    }
    _InterlockedExchange(writer_pstate_flag, true);
    AcquireSRWLockExclusive(&pl_state->srw_lock);
    switch (key_code) {
    case LOOP:
        pl_state->looping = !pl_state->looping;
        break;
    case CHAR_PRST:
        if (pl_state->preset_idx < 2) {
            pl_state->preset_idx++;
        } else {
            pl_state->preset_idx = 0;
        }
        break;
    case PLAYBACK:
        pl_state->playing = !pl_state->playing;
        break;
    case MUTE:
        pl_state->muted = !pl_state->muted;
        break;
    case VOL_UP:
        if (pl_state->vol_lvl < 1.0) {
            pl_state->vol_lvl += VOL_INCR;
        }
        break;
    case VOL_DOWN:
        if (pl_state->vol_lvl > VOL_INCR) {
            pl_state->vol_lvl -= VOL_INCR;
        }
        break;
    case F_FORWARD:
        pl_state->seeking                   = true;
        const double resulting_seek_idx_inc = pl_state->seek_multiple_idx + increment_magn;
        if (0 <= resulting_seek_idx_inc && resulting_seek_idx_inc <= SEEK_SPEED_MAX) {
            pl_state->seek_multiple_idx = resulting_seek_idx_inc;
        }
        const double resulting_mclock_inc =
            pl_state->main_clock + seek_speed_table[(size_t)floor(resulting_seek_idx_inc)];
        if (resulting_mclock_inc <= pl_conf->video_duration) {
            pl_state->main_clock = resulting_mclock_inc;
        } else {
            pl_state->main_clock = pl_conf->video_duration;
        }
        break;
    case F_BACKWARD:
        pl_state->seeking                   = true;
        const double resulting_seek_idx_dec = pl_state->seek_multiple_idx - increment_magn;
        if (0 <= resulting_seek_idx_dec && resulting_seek_idx_dec <= SEEK_SPEED_MAX) {
            pl_state->seek_multiple_idx = resulting_seek_idx_dec;
        }
        const double resulting_mclock_dec =
            pl_state->main_clock + seek_speed_table[(size_t)floor(resulting_seek_idx_dec)];
        if (0.0 <= resulting_mclock_dec) {
            pl_state->main_clock = resulting_mclock_dec;
        } else {
            pl_state->main_clock = 0.0;
        }
        break;
    default:
        pl_state->seeking           = false;
        pl_state->seek_multiple_idx = SEEK_SPEED_MAX / 2;
        break;
    }
    ReleaseSRWLockExclusive(&pl_state->srw_lock);
    _InterlockedExchange(writer_pstate_flag, false);
    return TPL_SUCCESS;
}
#endif