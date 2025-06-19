#include <stdio.h>
#include "tpl_app.h"
#include "tpl_errors.h"
#include "tpl_ffmpeg_utils.h"
#include "tpl_path.h"

tpl_result start_execution(wpath* video_path) {

    // Check path validity.
    wpath* resolved_path      = NULL;
    string* resolved_utf8path = NULL;

    tpl_result resolve_call = wpath_resolve_path(video_path, &resolved_path);
    if (tpl_failed(resolve_call)) {
        fwprintf(stderr, L"Path cannot be resolved.\n");
        LOG_ERR(resolve_call);
        return resolve_call;
    }
    bool exists = wpath_exists(resolved_path);
    if (!exists) {
        fwprintf(stderr, L"File does not exist or cannot be found.\n");
        LOG_ERR(TPL_INVALID_ARGUMENT);
        return TPL_INVALID_ARGUMENT;
    }

    tpl_result conv_call = wstr_to_utf8(resolved_path, &resolved_utf8path);
    if (tpl_failed(conv_call)) {
        LOG_ERR(conv_call);
        return conv_call;
    }

    // Check for valid a/v streams in file.
    bool streams_exist     = false;
    tpl_result exists_call = tpl_av_streams_exist(resolved_utf8path, &streams_exist);
    if (tpl_failed(exists_call)) {
        fwprintf(stderr, L"File is not in a supported format or is corrupted.\n");
        LOG_ERR(exists_call);
        return exists_call;
    }
    if (!streams_exist) {
        fwprintf(stderr, L"File A/V streams cannot be found.\n");
        LOG_ERR(TPL_GENERIC_ERROR);
        return TPL_GENERIC_ERROR;
    }

    // Test WriteConsoleA/fprintf.
    str_mulpush(resolved_utf8path, "\n");
    WriteConsoleA(
        GetStdHandle(STD_OUTPUT_HANDLE), str_c(resolved_utf8path), resolved_utf8path->buffer->len,
        NULL, NULL
    );
    fprintf(stdout, "\x1b[38;2;%d;%d;%dm%s\x1b[0m\n", 255, 55, 255, str_c(resolved_utf8path));
    return TPL_SUCCESS;

    // -----------------MUST DO--------------------
    // Make sure argv[1] is a valid path to a video
    // Make sure the video is in a supported format
    //---------------------------------------------

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
