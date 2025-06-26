#ifndef TERMIPLAY_PROC
#define TERMIPLAY_PROC
#define DEBUG

#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include "tpl_errors.h"
#include "tpl_player.h"
#define COLOR_CHANNELS 3

static tpl_result tpl_procfill_video_buffer(
    string**         video_buffer,
    const double     sec_start,
    const wpath*     video_fpath,
    const uint8_t    startup_conf_frate,
    const uint16_t   console_h,
    const uint16_t   console_w,
    tpl_player_conf* pl_config
) {
    tpl_result   return_code = TPL_SUCCESS;
    FILE*        pipe;
    tpl_wchar    command[1024];
    const size_t bytes_total = console_h * console_w * COLOR_CHANNELS * startup_conf_frate;

    // Get raw pixel data.
    uint8_t* data_buffer = malloc(bytes_total);
    _snwprintf(
        command, 1024,
        L"ffmpeg -hwaccel d3d11va -i \"%ls\" -ss %.3lf -f rawvideo -vf scale=%i:%i,fps=%i -pixfmt "
        L"rgb24 "
        L"-an -v error "
        L"-hide_banner -vframes %i -",
        wstr_c_const(video_fpath), sec_start, console_w, console_h, startup_conf_frate,
        startup_conf_frate
    );
    pipe = _wpopen(command, L"rb");
    IF_ERR_GOTO(pipe == NULL, TPL_FAILED_TO_PIPE, return_code);
    size_t bytes_read = 0;
    while (bytes_read < bytes_total) {
        size_t bread =
            fread(&data_buffer[bytes_read], sizeof(uint8_t), bytes_total - bytes_read, pipe);
        if (bread == 0) {
            break;
        }
        bytes_read += bread;
    }
    int exit_code = _pclose(pipe);
    IF_ERR_GOTO(exit_code != 0, TPL_PIPE_ERROR, return_code);

    // Get related config data.
    AcquireSRWLockShared(&pl_config->srw_lock);
    wstring*    wchar_preset1  = NULL;
    tpl_result wstr_init_call = wstr_init(&wchar_preset1);
    IF_ERR_GOTO(tpl_failed(wstr_init_call), wstr_init_call, return_code);
    tpl_result wstr_push_call = wstr_mulpush(wchar_preset1, wstr_c(pl_config->char_preset1));
    IF_ERR_GOTO(tpl_failed(wstr_push_call), wstr_push_call, return_code);

    tpl_wchar wchar_preset2   = pl_config->char_preset2;
    bool      frame_skip      = pl_config->frame_skip;
    uint8_t   frame_rate      = pl_config->frame_rate;
    bool      preserve_aspect = pl_config->preserve_aspect;
    bool      rgb_out         = pl_config->rgb_out;
    bool      gray_scale      = pl_config->gray_scale;
    ReleaseSRWLockShared(&pl_config->srw_lock);

    // NULL out all pointers as required for each string initialization.
    memset(video_buffer, 0, sizeof(string*) * startup_conf_frate);

    for (size_t i = 0; i < startup_conf_frate; ++i) {
        string*    frame          = NULL;
        tpl_result frame_initcall = str_init(&frame);
        IF_ERR_GOTO(tpl_failed(frame_initcall), frame_initcall, return_code);

        // Get individual frame from data_buffer
        size_t frame_data_start = i * COLOR_CHANNELS * console_h * console_w;
        size_t frame_data_end   = frame_data_start + (COLOR_CHANNELS * console_h * console_w);

        // Apply charset to frame
    }

unwind:
    free(data_buffer);
    data_buffer = NULL;
    return return_code;
}

static tpl_result tpl_execute_proc_thread(tpl_thread_data* thread_data) {
    const wpath* vfile_path      = thread_data->p_conf->video_fpath;
    double       thread_clock_ms = 0.0;

    while (true) {
        if (InterlockedOr(thread_data->shutdown_ptr, 0)) {
            return TPL_SUCCESS;
        }

        Sleep(1);
        thread_clock_ms += 1;
    }
}
#endif