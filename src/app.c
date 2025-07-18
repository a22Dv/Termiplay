#include "tl_app.h"
#include "tl_errors.h"
#include "tl_pch.h"
#include "tl_types.h"
#include "tl_utils.h"

tl_result player_exec(
    const int     argc,
    const WCHAR **wargv
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, argc != 2, TL_INVALID_ARG, return excv);
    CHECK(excv, wargv == NULL, TL_NULL_ARG, return excv);

    DWORD attr = GetFileAttributesW(wargv[1]);
    CHECK(excv, attr == INVALID_FILE_ATTRIBUTES, TL_INVALID_FILE, return excv);

    player *pl = NULL;
    TRY(excv, create_player(wargv[1], &pl), goto epilogue);

    /*
    Device-dependent. Audio playback depends on how fast the actual
    audio hardware reacts. A silent cut of the first fractions of a second
    will happen especially with old Bluetooth devices.
    */

    set_atomic_bool(&pl->looping, true);
    set_atomic_bool(&pl->playing, true);
    set_atomic_double(&pl->volume, 0.5);

    DWORD th_excv = 0;
    while (true) {
        if (get_atomic_bool(&pl->shutdown)) {
            break;
        }
        key_code key = NO_INPUT;
        get_input(&key);
        TRY(excv, process_input(pl, key), goto epilogue);

        // Check for premature terminations.
        for (size_t i = 0; i < pl->active_threads; ++i) {
            if (WaitForSingleObject(pl->th_hndles[i], 0) != WAIT_TIMEOUT) {
                GetExitCodeThread(pl->th_hndles[i], &th_excv);
                excv = th_excv;
                goto epilogue;
            }
        }
        Sleep(POLLING_RATE_MS);
    }
epilogue:
    if (pl) {
        destroy_player(&pl);
    }
    return excv;
}

void get_input(key_code *kc) {
    if (kc == NULL) {
        return;
    }
    if (!_kbhit()) {
        *kc = NO_INPUT;
        return;
    }
    int ch = _getch();
    if (ch != EXT_KEYCODE) {
        switch (ch) {
        case 'l':
            *kc = L;
            break;
        case ' ':
            *kc = SPACE;
            break;
        case 'm':
            *kc = M;
            break;
        case 'q':
            *kc = Q;
            break;
        case 'g':
            *kc = G;
            break;
        case 'd':
            *kc = D;
            break;
        case 'r':
            *kc = R;
            break;
        default:
            *kc = NO_INPUT;
            break;
        }
        goto epilogue;
    }
    int ext_ch = _getch();
    switch (ext_ch) {
    case ARR_UP_KEYC:
        *kc = ARR_UP;
        break;
    case ARR_DOWN_KEYC:
        *kc = ARR_DOWN;
        break;
    case ARR_LEFT_KEYC:
        *kc = ARR_LEFT;
        break;
    case ARR_RIGHT_KEYC:
        *kc = ARR_RIGHT;
        break;
    default:
        *kc = NO_INPUT;
        break;
    }
epilogue:
    while (_kbhit()) {
        _getch();
    }
}

tl_result process_input(
    player        *pl,
    const key_code kc
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, pl == NULL, TL_NULL_ARG, return excv);

    static double                     seek_length_s = 0.0;
    static const double               ticks_ps = (double)1 / ((double)POLLING_RATE_MS / 1000.0);
    static double                     spdl_unbounded = 0.0;
    static double                     spdr_unbounded = 0.0;
    static bool                       startup = true;
    static bool                       playback_stats_set = true;
    static CONSOLE_SCREEN_BUFFER_INFO set_csbi;

    if (startup) {
        const HANDLE stdouth = GetStdHandle(STD_OUTPUT_HANDLE);
        CHECK(excv, stdouth == NULL || stdouth == INVALID_HANDLE_VALUE, TL_OS_ERR, return excv);
        CHECK(excv, !GetConsoleScreenBufferInfo(stdouth, &set_csbi), TL_CONSOLE_ERR, return excv);
        startup = false;
    }

    CONSOLE_SCREEN_BUFFER_INFO new_csbi;
    const HANDLE               stdouth = GetStdHandle(STD_OUTPUT_HANDLE);
    CHECK(excv, stdouth == NULL || stdouth == INVALID_HANDLE_VALUE, TL_OS_ERR, return excv);
    CHECK(excv, !GetConsoleScreenBufferInfo(stdouth, &new_csbi), TL_CONSOLE_ERR, return excv);

    if (new_csbi.srWindow.Bottom != set_csbi.srWindow.Bottom ||
        new_csbi.srWindow.Top != set_csbi.srWindow.Top ||
        new_csbi.srWindow.Left != set_csbi.srWindow.Left ||
        new_csbi.srWindow.Right != set_csbi.srWindow.Right) {
        set_atomic_bool(&pl->invalidated, true);
        add_atomic_size_t(&pl->serial, 1);
        set_csbi = new_csbi;
        return TL_SUCCESS;
    }

    AcquireSRWLockExclusive(&pl->srw_mclock);

    // We can't seek beyond the end without possibly raising errors, so we cut it a bit short
    // (~50ms)
    if (get_atomic_double(&pl->main_clock) > pl->media_mtdta->duration - POLLING_RATE_S) {
        if (get_atomic_bool(&pl->looping)) {
            set_atomic_bool(&pl->invalidated, true);
            set_atomic_double(&pl->main_clock, 0.0);
        } else {
            set_atomic_bool(&pl->shutdown, true);
        }
        add_atomic_size_t(&pl->serial, 1);
    }
    ReleaseSRWLockExclusive(&pl->srw_mclock);
    if (get_atomic_bool(&pl->shutdown)) {
        return excv;
    }
    if (!get_atomic_bool(&pl->debug_print)) {
        playback_stats(pl);
    }
    switch (kc) {
    case Q:
        set_atomic_bool(&pl->shutdown, true);
        break;
    case SPACE:
        flip_atomic_bool(&pl->playing);
        break;
    case M:
        flip_atomic_bool(&pl->muted);
        break;
    case L:
        flip_atomic_bool(&pl->looping);
        break;
    case ARR_UP:
        set_atomic_double(
            &pl->volume, get_atomic_double(&pl->volume) + 0.02 < 1.01
                             ? get_atomic_double(&pl->volume) + 0.02
                             : 1.0
        );
        if (!get_atomic_bool(&pl->debug_print)) {
            playback_stats(pl);
        }
        break;
    case ARR_DOWN:
        set_atomic_double(
            &pl->volume, get_atomic_double(&pl->volume) - 0.02 > -0.01
                             ? get_atomic_double(&pl->volume) - 0.02
                             : 0.0
        );
        if (!get_atomic_bool(&pl->debug_print)) {
            playback_stats(pl);
        }
        break;
    case ARR_LEFT:
        spdl_unbounded = 1.8 * pow(2, seek_length_s);
        AcquireSRWLockExclusive(&pl->srw_mclock);
        set_atomic_bool(&pl->invalidated, true);
        const double ssl = get_atomic_double(&pl->seek_speed);
        if (ssl == 0.0) {
            add_atomic_size_t(&pl->serial, 1);
        }
        set_atomic_double(&pl->seek_speed, spdl_unbounded > 480.0 ? 480.0 : spdl_unbounded);
        if (get_atomic_double(&pl->main_clock) - ssl > 0.0) {
            add_atomic_double(&pl->main_clock, -ssl);
        } else {
            set_atomic_double(&pl->main_clock, 0.0);
        }
        ReleaseSRWLockExclusive(&pl->srw_mclock);
        if (!get_atomic_bool(&pl->debug_print)) {
            playback_stats(pl);
        }
        seek_length_s += POLLING_RATE_S;
        break;
    case ARR_RIGHT:
        spdr_unbounded = 1.8 * pow(2, seek_length_s);
        AcquireSRWLockExclusive(&pl->srw_mclock);
        set_atomic_bool(&pl->invalidated, true);
        const double ssr = get_atomic_double(&pl->seek_speed);
        if (ssr == 0.0) {
            add_atomic_size_t(&pl->serial, 1);
        }
        set_atomic_double(&pl->seek_speed, spdr_unbounded > 480.0 ? 480.0 : spdr_unbounded);
        add_atomic_double(&pl->main_clock, ssr);
        ReleaseSRWLockExclusive(&pl->srw_mclock);
        if (!get_atomic_bool(&pl->debug_print)) {
            playback_stats(pl);
        }
        seek_length_s += POLLING_RATE_S;
        break;
    case G:
        flip_atomic_bool(&pl->debug_print);
        break;
    case D:
        const size_t dth_cmode = get_atomic_size_t(&pl->dither_mode);
        if (dth_cmode == DTH_MODES - 1) {
            set_atomic_size_t(&pl->dither_mode, 0);
        } else {
            add_atomic_size_t(&pl->dither_mode, 1);
        }
        break;
    case R:
        const size_t clr_cmode = get_atomic_size_t(&pl->color_mode);
        if (clr_cmode == 1) {
            set_atomic_size_t(&pl->color_mode, CLM_WHITE);
        } else {
             const int8_t clr_nmode = (int8_t)clr_cmode - 1;
             set_atomic_size_t(&pl->color_mode, clr_nmode);
        }
    case NO_INPUT:
        set_atomic_double(&pl->seek_speed, 0.0);
        set_atomic_bool(&pl->invalidated, false);
        seek_length_s = 0.0;
        break;
    }
    if (get_atomic_bool(&pl->debug_print)) {
        state_print(pl);
    }
    return excv;
}