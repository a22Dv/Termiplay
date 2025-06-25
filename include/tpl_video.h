#ifndef TERMIPLAY_VIDEO
#define TERMIPLAY_VIDEO
#define DEBUG
#define COLOR_CHANNELS 3

#include <stdint.h>
#include <stdio.h>
#include "tpl_errors.h"
#include "tpl_player.h"

static tpl_result tpl_execute_video_thread(tpl_thread_data* thread_data) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    while (true) {
        if (InterlockedOr(thread_data->shutdown_ptr, 0)) {
            return TPL_SUCCESS;
        }
        if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
            LOG_ERR(TPL_FAILED_TO_GET_CONSOLE_DIMENSIONS);
            return TPL_FAILED_TO_GET_CONSOLE_DIMENSIONS;
        }
        // Dimensions.
        const uint16_t height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        const uint16_t width  = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        Sleep(1);
    }
}

static tpl_result tpl_fill_video_buffer(
    const double   sec_start,
    string**       video_buffer,
    const wpath*   video_fpath,
    const uint8_t  video_frame_rate,
    const uint16_t h_resolution,
    const uint16_t w_resolution
) {
    tpl_result return_code = TPL_SUCCESS;
    FILE*        pipe;
    tpl_wchar    command[1024];
    const size_t bytes_total = h_resolution * w_resolution * COLOR_CHANNELS * video_frame_rate;
    uint8_t*     data_buffer = malloc(bytes_total);
    _snwprintf(
        command, 1024,
        L"ffmpeg -hwaccel d3d11va -i \"%ls\" -ss %.3lf -f rawvideo -vf scale=%i:%i -pixfmt rgb24 "
        L"-an -v error "
        L"-hide_banner -vframes %i -",
        wstr_c_const(video_fpath), sec_start, w_resolution, h_resolution, video_frame_rate
    );
    pipe = _wpopen(command, L"rb");
    IF_ERR_GOTO(pipe == NULL, TPL_FAILED_TO_PIPE, return_code);
    size_t bytes_read = 0;
    while (bytes_read < bytes_total) {
        size_t bread = fread(&data_buffer[bytes_read], sizeof(uint8_t), bytes_total - bytes_read, pipe);
        if (bread == 0) {
            break;
        }
        bytes_read += bread;
    }
    int exit_code = _pclose(pipe);
    IF_ERR_GOTO(exit_code != 0, TPL_PIPE_ERROR, return_code);
    // check excalidraw for next steps.



unwind:
    free(data_buffer);
    data_buffer = NULL;
    return return_code;
}
#endif