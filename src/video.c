#include "tl_video.h"

tl_result video_thread_exec(thread_data *thdata) {
    tl_result exit_code = TL_SUCCESS;
    double    pts_buffer[VIDEO_FPS];
    size_t    current_idx = 0;
    size_t    set_serial = 0;
    for (size_t i = 0; i < VIDEO_FPS; ++i) {
        if (i == 0) {
            pts_buffer[i] = (double)0.0;
            continue;
        }
        pts_buffer[i] = pts_buffer[i - 1] + (double)1.0 / (double)VIDEO_FPS;
    }
    /// TODO: A/V Sync -> Frame decompressing -> Frame Display -> Dithering -> Polish -> RGB
    /// support.

    // Execution loop.
    while (true) {
        AcquireSRWLockShared(&thdata->pstate->control_srw);
        const bool   shutdown = thdata->pstate->shutdown;
        ReleaseSRWLockShared(&thdata->pstate->control_srw);
        
        AcquireSRWLockShared(&thdata->pstate->buffer_serial_srw);
        AcquireSRWLockShared(&thdata->pstate->clock_srw);
        const size_t cur_serial = thdata->pstate->current_serial;
        const bool   vbuf1_readable = thdata->pstate->vbuffer1_readable;
        const double main_clock = thdata->pstate->main_clock;
        ReleaseSRWLockShared(&thdata->pstate->clock_srw);
        ReleaseSRWLockShared(&thdata->pstate->buffer_serial_srw);
    
        if (shutdown) {
            goto epilogue;
        }
        if (cur_serial != set_serial) {
            get_new_pts(pts_buffer, main_clock);
            set_serial = cur_serial;
            current_idx = 0;
            continue;
        }
        if (vbuf1_readable == false) {
            Sleep(1);
            continue;
        }

        // Deep copies only happen during regular playback. Seeking is handled by a separate
        // producer thread. Must free after a frame is consumed regardless.
        if (current_idx == VIDEO_FPS - 1) {
            get_new_pts(pts_buffer, main_clock);
            while (true) {
                
                // Possible race-condition. Heavily relies on the fact that once
                // the flag is set to true, this is the only place where
                // it can be set to false. Monitor. dcopy might copy NULL-ed
                // data resulting in null-dereference if very unlucky.
                AcquireSRWLockShared(&thdata->pstate->buffer_serial_srw);
                const bool vbuf2_readable = thdata->pstate->vbuffer2_readable;
                ReleaseSRWLockShared(&thdata->pstate->buffer_serial_srw);
                if (vbuf2_readable) {
                    // Deep memcpy. ~1.2ms cost estimate.
                    TRY(exit_code,
                        dcopy_buffer_contents(thdata->video_buffer2, thdata->video_buffer1),
                        goto epilogue);
                } else {
                    Sleep(1);
                    continue;
                }
                AcquireSRWLockExclusive(&thdata->pstate->buffer_serial_srw);
                thdata->pstate->vbuffer2_readable = false;
                ReleaseSRWLockExclusive(&thdata->pstate->buffer_serial_srw);
                break;
            }
            current_idx = 0;
        }
        const double drift = main_clock - pts_buffer[current_idx];
        if (drift < 0) {
            Sleep(5);
            continue;
        } else if (drift > (double)2 / VIDEO_FPS) {
            current_idx += 1;
            continue;
        }
        // TODO: Display frame.
        free(thdata->video_buffer1[current_idx]);
        thdata->video_buffer1[current_idx] = NULL;
        current_idx++;
        Sleep(1);
    }
epilogue:
    return exit_code;
}

void get_new_pts(
    double *pts_buf,
    double  start_clock
) {
    if (pts_buf == NULL) {
        return;
    }
    pts_buf[0] = start_clock;
    for (size_t i = 1; i < VIDEO_FPS; ++i) {
        pts_buf[i] = pts_buf[i - 1] + (double)1 / VIDEO_FPS;
    }
}

tl_result dcopy_buffer_contents(
    const char **src,
    char       **dst
) {
    tl_result exit_code = TL_SUCCESS;
    CHECK(exit_code, src == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, dst == NULL, TL_NULL_ARGUMENT, return exit_code);
    int data_bytes = 0;
    memset(dst, 0, sizeof(char *) * VIDEO_FPS);
    for (size_t i = 0; i < VIDEO_FPS; ++i) {
        memcpy(&data_bytes, src[i], sizeof(int));
        dst[i] = malloc(sizeof(int) * 2 + (size_t)data_bytes);
        CHECK(exit_code, dst[i] == NULL, TL_ALLOC_FAILURE, goto epilogue);
        memcpy(dst[i], src[i], sizeof(int) * 2 + (size_t)data_bytes);
    }
epilogue:
    if (exit_code != TL_SUCCESS) {
        for (size_t i = 0; i < VIDEO_FPS; ++i) {
            free(dst[i]);
            dst[i] = NULL;
        }
    }
    return exit_code;
}