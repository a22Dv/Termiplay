#include "tl_video.h"

tl_result video_thread_exec(thread_data *thdata) {
    tl_result exit_code = TL_SUCCESS;
    char    **video_buffer = thdata->video_buffer1;
    double    pts_buffer[VIDEO_FPS];

    /// TODO: Calculate PTS, decode frame, flush to screen.
    for (size_t i = 0; i < VIDEO_FPS; ++i) {
        if (i == 0) {
            pts_buffer[i] = (double)0.0;
            continue;
        }
        pts_buffer[i] = pts_buffer[i - 1] + (double)1.0 / (double)VIDEO_FPS;
    }
    while (true) {
        AcquireSRWLockShared(&thdata->pstate->srw);
        const bool shutdown_sig = thdata->pstate->shutdown;
        ReleaseSRWLockShared(&thdata->pstate->srw);
        if (shutdown_sig) {
            break;
        }
        Sleep(10);
    }
    return exit_code;
}