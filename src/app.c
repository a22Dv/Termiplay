#include <stdio.h>
#include "tpl_app.h"
#include "tpl_errors.h"
#include "tpl_path.h"

tpl_result start_execution(wpath* video_path) {
    // Check path validity.
    wpath* resolved_path = NULL;
    tpl_result resolve_call = wpath_resolve_path(video_path, &resolved_path);
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
