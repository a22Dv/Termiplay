#ifndef TERMIPLAY_VIDEO
#define TERMIPLAY_VIDEO
#define DEBUG

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

#endif