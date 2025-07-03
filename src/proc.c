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
    tl_result    exit_code = TL_SUCCESS;
    int16_t     *target_buffer = worker_thdata->wthread_id == WORKER_A1
                                     ? worker_thdata->thdata->audio_buffer1
                                     : worker_thdata->thdata->audio_buffer2;
    bool        *target_readable_flag = worker_thdata->wthread_id == WORKER_A1
                                            ? &worker_thdata->thdata->pstate->abuffer1_readable
                                            : &worker_thdata->thdata->pstate->abuffer2_readable;
    HANDLE       event_flag = worker_thdata->order_event;
    HANDLE       finished_flag = worker_thdata->finish_event;
    HANDLE       shutdown_flag = worker_thdata->shutdown_event;
    const HANDLE wake_flags[2] = {event_flag, shutdown_flag};
    const WCHAR *media_path = worker_thdata->thdata->mtdta->media_path;
    while (true) {
        WaitForMultipleObjects(2, wake_flags, FALSE, INFINITE);
        AcquireSRWLockShared(&worker_thdata->thdata->pstate->srw);
        const size_t serial = worker_thdata->thdata->pstate->current_serial;
        const bool   shutdown = worker_thdata->thdata->pstate->shutdown;
        const double clock_start = worker_thdata->thdata->pstate->main_clock;
        ReleaseSRWLockShared(&worker_thdata->thdata->pstate->srw);
        if (shutdown || WaitForSingleObject(shutdown_flag, 0) == WAIT_OBJECT_0) {
            break;
        }
        memset(target_buffer, 0, sizeof(int16_t) * AUDIO_SAMPLE_RATE * CHANNEL_COUNT);
        TRY(exit_code,
            write_abuffer(
                serial, worker_thdata->thdata->pstate, clock_start + (worker_thdata->wthread_id == WORKER_A1 ? 0.0 : 1.0), media_path, target_buffer
            ),
            break);
        AcquireSRWLockExclusive(&worker_thdata->thdata->pstate->srw);
        if (worker_thdata->thdata->pstate->current_serial == serial) {
            *target_readable_flag = true;
        }
        ReleaseSRWLockExclusive(&worker_thdata->thdata->pstate->srw);
        SetEvent(finished_flag);
    }
    return exit_code;
}

tl_result video_helper_thread_exec(wthread_data *worker_thdata) {
    tl_result    exit_code = TL_SUCCESS;
    char       **target_buffer = worker_thdata->wthread_id == WORKER_V1
                                     ? worker_thdata->thdata->video_buffer1
                                     : worker_thdata->thdata->video_buffer2;
    bool        *target_readable_flag = worker_thdata->wthread_id == WORKER_V1
                                            ? &worker_thdata->thdata->pstate->vbuffer1_readable
                                            : &worker_thdata->thdata->pstate->vbuffer2_readable;
    HANDLE       event_flag = worker_thdata->order_event;
    HANDLE       finished_flag = worker_thdata->finish_event;
    HANDLE       shutdown_flag = worker_thdata->shutdown_event;
    const HANDLE wake_flags[2] = {event_flag, shutdown_flag};
    const WCHAR *media_path = worker_thdata->thdata->mtdta->media_path;
    while (true) {
        WaitForMultipleObjects(2, wake_flags, FALSE, INFINITE);
        AcquireSRWLockShared(&worker_thdata->thdata->pstate->srw);
        const bool   shutdown = worker_thdata->thdata->pstate->shutdown;
        const size_t serial = worker_thdata->thdata->pstate->current_serial;
        const double clock_start = worker_thdata->thdata->pstate->main_clock;
        const bool   default_charset = worker_thdata->thdata->pstate->default_charset;
        ReleaseSRWLockShared(&worker_thdata->thdata->pstate->srw);
        if (shutdown || WaitForSingleObject(shutdown_flag, 0) == WAIT_OBJECT_0) {
            break;
        }
        for (size_t i = 0; i < VIDEO_FPS; ++i) {
            free(target_buffer[i]);
            target_buffer[i] = NULL;
        }
        TRY(exit_code,
            write_vbuffer_compressed(
                serial, worker_thdata->thdata->pstate, clock_start + (worker_thdata->wthread_id == WORKER_V1 ? 0.0 : 1.0), media_path, target_buffer,
                default_charset
            ),
            break);
        AcquireSRWLockExclusive(&worker_thdata->thdata->pstate->srw);
        if (worker_thdata->thdata->pstate->current_serial == serial) {
            *target_readable_flag = true;
        }
        ReleaseSRWLockExclusive(&worker_thdata->thdata->pstate->srw);
        SetEvent(finished_flag);
    }
    for (size_t i = 0; i < VIDEO_FPS; ++i) {
        free(target_buffer[i]);
        target_buffer[i] = NULL;
    }
    return exit_code;
}
