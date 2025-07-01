#include "tl_proc.h"

tl_result write_abuffer(
    const size_t  serial_at_call,
    player_state *pstate,
    const double  clock_start,
    const WCHAR  *media_path,
    int16_t      *buffer
) {
    return TL_SUCCESS;
}

tl_result write_vbuffer_compressed(
    const size_t  serial_at_call,
    player_state *pstate,
    const double  clock_start,
    const WCHAR  *media_path,
    char        **buffer,
    const bool    default_charset
) {
    Sleep(350);
    return TL_SUCCESS;
}

tl_result audio_helper_thread_exec(wthread_data *worker_thdata) {
    tl_result exit_code = TL_SUCCESS;
    int16_t  *target_buffer = worker_thdata->wthread_id == WORKER_A1
                                  ? worker_thdata->thdata->audio_buffer1
                                  : worker_thdata->thdata->audio_buffer2;
    bool     *target_readable_flag = worker_thdata->wthread_id == WORKER_A1
                                         ? &worker_thdata->thdata->pstate->abuffer1_readable
                                         : &worker_thdata->thdata->pstate->abuffer2_readable;
    HANDLE   event_flag = worker_thdata->order_event;
    while (true) {
        WaitForSingleObject(event_flag, INFINITE);
        AcquireSRWLockShared(&worker_thdata->thdata->pstate->srw);
        bool shutdown = worker_thdata->thdata->pstate->shutdown;
        ReleaseSRWLockShared(&worker_thdata->thdata->pstate->srw);
        if (shutdown) {
            break;
        }
        // Free buffer it was called on.
        // Call function.
    }
    return exit_code;
}

tl_result video_helper_thread_exec(wthread_data *worker_thdata) {
    tl_result exit_code = TL_SUCCESS;
    char  **target_buffer = worker_thdata->wthread_id == WORKER_V1
                                  ? worker_thdata->thdata->video_buffer1
                                  : worker_thdata->thdata->video_buffer2;
    bool     *target_readable_flag = worker_thdata->wthread_id == WORKER_V1
                                         ? &worker_thdata->thdata->pstate->vbuffer1_readable
                                         : &worker_thdata->thdata->pstate->vbuffer2_readable;
    HANDLE   event_flag = worker_thdata->order_event;
    while (true) {
        WaitForSingleObject(event_flag, INFINITE);
        AcquireSRWLockShared(&worker_thdata->thdata->pstate->srw);
        bool shutdown = worker_thdata->thdata->pstate->shutdown;
        ReleaseSRWLockShared(&worker_thdata->thdata->pstate->srw);
        if (shutdown) {
            break;
        }
        // Free buffer it was called on.
        // Call function.
    }
    return exit_code;
}
