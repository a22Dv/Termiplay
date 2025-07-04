#include "tl_proc.h"

tl_result proc_thread_exec(thread_data *thdata) {
    tl_result exit_code = TL_SUCCESS;
    CHECK(exit_code, thdata == NULL, TL_NULL_ARGUMENT, return exit_code);

    const double  media_duration = thdata->mtdta->duration;
    const WCHAR  *media_path = thdata->mtdta->media_path;
    const uint8_t media_streams = thdata->mtdta->streams_mask;
    const size_t  buffer_count = MAX_BUFFER_COUNT - ((media_streams & VIDEO_PRESENT ? 0 : 1) * 2);
    size_t        serial_acknowledged = 0;

    HANDLE active_finished_ehandle[MAX_BUFFER_COUNT] = {NULL, NULL, NULL, NULL};
    HANDLE active_wthread_handles[MAX_BUFFER_COUNT] = {NULL, NULL, NULL, NULL};
    size_t active_wthread_count = 0;

    // Set handles & worker thread data.
    HANDLE        *finished_ehandle = NULL;
    HANDLE        *shutdown_ehandle = NULL;
    HANDLE        *order_ehandle = NULL;
    HANDLE        *worker_thread_handles = NULL;
    wthread_data **worker_thread_data = NULL;
    uint8_t        initialized_wthreads = 0;

    // Buffer count is used here as each thread is responsible for one buffer.
    finished_ehandle = malloc(sizeof(HANDLE) * buffer_count);
    order_ehandle = malloc(sizeof(HANDLE) * buffer_count);
    shutdown_ehandle = malloc(sizeof(HANDLE) * buffer_count);
    worker_thread_handles = malloc(sizeof(HANDLE) * buffer_count);
    worker_thread_data = malloc(sizeof(wthread_data *) * buffer_count);
    CHECK(exit_code, finished_ehandle == NULL, TL_ALLOC_FAILURE, goto epilogue);
    CHECK(exit_code, order_ehandle == NULL, TL_ALLOC_FAILURE, goto epilogue);
    CHECK(exit_code, shutdown_ehandle == NULL, TL_ALLOC_FAILURE, goto epilogue);
    CHECK(exit_code, worker_thread_handles == NULL, TL_ALLOC_FAILURE, goto epilogue);
    CHECK(exit_code, worker_thread_data == NULL, TL_ALLOC_FAILURE, goto epilogue);
    memset(finished_ehandle, 0, sizeof(HANDLE) * buffer_count);
    memset(order_ehandle, 0, sizeof(HANDLE) * buffer_count);
    memset(shutdown_ehandle, 0, sizeof(HANDLE) * buffer_count);
    memset(worker_thread_handles, 0, sizeof(HANDLE) * buffer_count);
    memset(worker_thread_data, 0, sizeof(wthread_data *) * buffer_count);

    const uint8_t thread_ids[MAX_BUFFER_COUNT] = {WORKER_A1, WORKER_A2, WORKER_V1, WORKER_V2};
    for (size_t i = 0; i < buffer_count; ++i) {
        finished_ehandle[i] = CreateEventW(NULL, TRUE, FALSE, NULL);
        CHECK(exit_code, finished_ehandle[i] == INVALID_HANDLE_VALUE, TL_OS_ERROR, goto epilogue);
        order_ehandle[i] = CreateEventW(NULL, FALSE, FALSE, NULL);
        CHECK(exit_code, order_ehandle[i] == INVALID_HANDLE_VALUE, TL_OS_ERROR, goto epilogue);
        shutdown_ehandle[i] = CreateEventW(NULL, TRUE, FALSE, NULL);
        CHECK(exit_code, shutdown_ehandle[i] == INVALID_HANDLE_VALUE, TL_OS_ERROR, goto epilogue);
        TRY(exit_code,
            create_wthread_data(
                thdata, order_ehandle[i], finished_ehandle[i], shutdown_ehandle[i], thread_ids[i],
                &worker_thread_data[i]
            ),
            goto epilogue);
        worker_thread_handles[i] = (HANDLE)_beginthreadex(
            NULL, 0, helper_thread_dispatcher, (void *)worker_thread_data[i], 0, NULL
        );
        CHECK(exit_code, worker_thread_handles[i] == NULL, TL_OS_ERROR, goto epilogue);
        initialized_wthreads++;
    }

    while (true) {
        active_wthread_count = 0;
        AcquireSRWLockShared(&thdata->pstate->srw);
        const bool   shutdown_status = thdata->pstate->shutdown;
        const bool   current_charset = thdata->pstate->default_charset;
        const bool   current_seek_state = thdata->pstate->seeking;
        const double current_clock = thdata->pstate->main_clock;
        const size_t current_serial = thdata->pstate->current_serial;
        ReleaseSRWLockShared(&thdata->pstate->srw);

        if (shutdown_status) {
            break;
        }
        // Buffer invalidation.
        if (current_serial != serial_acknowledged) {
            AcquireSRWLockExclusive(&thdata->pstate->srw);
            thdata->pstate->abuffer1_readable = false;
            thdata->pstate->abuffer2_readable = false;
            thdata->pstate->vbuffer1_readable = false;
            thdata->pstate->vbuffer2_readable = false;
            ReleaseSRWLockExclusive(&thdata->pstate->srw);
            serial_acknowledged = current_serial;
        }
        if (current_seek_state) {
            // Sleep proportional to the polling rate to avoid wasting time
            // but prevent a tight busy-waiting loop.
            Sleep(POLLING_RATE_MS / 5);
            continue;
        }
        AcquireSRWLockShared(&thdata->pstate->srw);
        const bool readable_state[MAX_BUFFER_COUNT] = {
            thdata->pstate->abuffer1_readable, thdata->pstate->abuffer2_readable,
            thdata->pstate->vbuffer1_readable, thdata->pstate->vbuffer2_readable
        };
        ReleaseSRWLockShared(&thdata->pstate->srw);
        for (size_t i = 0; i < buffer_count; ++i) {
            if (readable_state[i]) {
                continue;
            }
            SetEvent(order_ehandle[i]);
            active_finished_ehandle[active_wthread_count] = finished_ehandle[i];
            active_wthread_handles[active_wthread_count] = worker_thread_handles[i];
            active_wthread_count++;
        }
        if (active_wthread_count == 0) {
            Sleep(1);
            continue;
        }

        for (size_t i = 0; i < active_wthread_count; ++i) {
            bool  succeeded = false;
            DWORD terminated = WAIT_TIMEOUT;
            while (terminated == WAIT_TIMEOUT && !succeeded) {
                terminated = WaitForSingleObject(active_wthread_handles[i], 10);
                if (terminated == WAIT_TIMEOUT) {
                    succeeded = WaitForSingleObject(active_finished_ehandle[i], 0) == WAIT_OBJECT_0;
                }
            }
            if (terminated != WAIT_TIMEOUT) {
                HANDLE pexit_handle = active_wthread_handles[i];
                DWORD  wexit_code = 0;
                CHECK(
                    exit_code, !GetExitCodeThread(pexit_handle, &wexit_code), TL_OS_ERROR,
                    goto epilogue
                );
                exit_code = (tl_result)wexit_code;
                for (size_t j = 0; j < buffer_count; ++j) {
                    SetEvent(shutdown_ehandle[j]);
                }
                goto epilogue;
            }
            ResetEvent(active_finished_ehandle[i]);
        }
    }
epilogue:
    for (size_t i = 0; i < initialized_wthreads; ++i) {
        SetEvent(shutdown_ehandle[i]);
    }
    if (initialized_wthreads > 0) {
        WaitForMultipleObjects((DWORD)initialized_wthreads, worker_thread_handles, TRUE, INFINITE);
    }
    for (size_t i = 0; i < buffer_count; ++i) {
        if (worker_thread_handles && worker_thread_handles[i]) {
            CloseHandle(worker_thread_handles[i]);
        }
        if (order_ehandle && order_ehandle[i]) {
            CloseHandle(order_ehandle[i]);
        }
        if (shutdown_ehandle && shutdown_ehandle[i]) {
            CloseHandle(shutdown_ehandle[i]);
        }
        if (finished_ehandle && finished_ehandle[i]) {
            CloseHandle(finished_ehandle[i]);
        }
        if (worker_thread_data && worker_thread_data[i]) {
            free(worker_thread_data[i]);
        }
    }
    free(finished_ehandle);
    free(order_ehandle);
    free(shutdown_ehandle);
    free(worker_thread_handles);
    free(worker_thread_data);
    return exit_code;
}

tl_result write_abuffer(
    const size_t  serial_at_call,
    player_state *pstate,
    const double  clock_start,
    const WCHAR  *media_path,
    const uint8_t streams,
    int16_t      *buffer
) {
    tl_result exit_code = TL_SUCCESS;
    CHECK(exit_code, pstate == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, media_path == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, buffer == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, clock_start < 0.0, TL_INVALID_ARGUMENT, return exit_code);
    wchar_t command[COMMAND_BUFFER_SIZE];

    int ret = swprintf(
        command, COMMAND_BUFFER_SIZE,
        L"ffmpeg -ss %lf -i \"%ls\" -t 1 -f s16le -acodec pcm_s16le -ar %i -ac %i %s -v error - "
        L"2>NUL",
        clock_start, media_path, AUDIO_SAMPLE_RATE, CHANNEL_COUNT,
        streams & VIDEO_PRESENT ? L"-vn" : L""
    );
    CHECK(exit_code, ret >= COMMAND_BUFFER_SIZE || ret < 0, TL_FORMAT_FAILURE, goto epilogue);

    FILE *pipe = NULL;
    pipe = _wpopen(command, L"rb");
    CHECK(exit_code, pipe == NULL, TL_PIPE_OPEN_FAILURE, goto epilogue);

    size_t bytes_read = 0;
    while (true) {
        size_t fbytes = fread(
            buffer + bytes_read, sizeof(char),
            sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE - bytes_read, pipe
        );
        if (fbytes == 0) {
            break;
        }
        bytes_read += fbytes;
    }
    memset(
        (char *)buffer + bytes_read, 0,
        sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE - bytes_read
    );
    int pexit_code = _pclose(pipe);
    CHECK(exit_code, pexit_code != 0 && bytes_read == 0, TL_PIPE_PROCESS_FAILURE, goto epilogue);
epilogue:
    if (exit_code != TL_SUCCESS) {
        memset(buffer, 0, sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);
    }
    return exit_code;
}

tl_result write_vbuffer_compressed(
    const size_t serial_at_call,
    thread_data *thdata,
    const double clock_start,
    const WCHAR *media_path,

    const uint8_t streams,
    char        **buffer,
    const bool    default_charset
) {
    tl_result exit_code = TL_SUCCESS;
    CHECK(exit_code, thdata == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, media_path == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, buffer == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, !(streams & VIDEO_PRESENT), TL_INVALID_ARGUMENT, return exit_code);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

    // +1 for inclusive coordinates. * 2 as we use half-blocks for rendering.
    const size_t cheight = (csbi.srWindow.Bottom - csbi.srWindow.Top + 1) * 2;
    const size_t cwidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    const size_t vheight = thdata->mtdta->height;
    const size_t vwidth = thdata->mtdta->width;

    // Calculate aspect preserving box.
    const double vaspect = (double)vwidth / (double)vheight;
    const double caspect = (double)cwidth / (double)cheight;
    size_t       f_width = 0;
    size_t       f_height = 0;

    if (caspect < vaspect) {
        f_width = cwidth;
        f_height = (size_t)((double)cwidth / (double)vaspect);

    } else {
        f_height = cheight;
        f_width = (size_t)((double)cheight * (double)vaspect);
    }
    if (f_height & 1) {
        // XOR-out to always have even numbers. Character cells are 1x2 "effective pixels".
        f_height ^= 1;
    }

    wchar_t command[COMMAND_BUFFER_SIZE];
    int     ret = swprintf(
        command, COMMAND_BUFFER_SIZE,
        L"ffmpeg -ss %lf -i \"%ls\" -vframes %i -r %i -vf scale=%llu:%llu:flags=%ls -pix_fmt %ls "
        L"-an "
        L"-v error -",
        clock_start, media_path, VIDEO_FPS, VIDEO_FPS, f_width, f_height, L"lanczos",
        default_charset ? L"gray" : L"rgb24"
    );
    CHECK(exit_code, ret < 0 || ret >= COMMAND_BUFFER_SIZE, TL_FORMAT_FAILURE, return exit_code);

    FILE *pipe = NULL;
    pipe = _wpopen(command, L"rb");
    CHECK(exit_code, pipe == NULL, TL_PIPE_OPEN_FAILURE, return exit_code);

    const size_t color_channels = default_charset ? 1 : 3;
    const size_t bytes_expected_per_frame = f_height * f_width * color_channels * sizeof(uint8_t);
    uint8_t      frames_processed = 0;
    uint8_t     *reference_fbuffer = malloc(bytes_expected_per_frame);
    uint8_t     *frame_buffer = malloc(bytes_expected_per_frame);
    CHECK(exit_code, frame_buffer == NULL, TL_ALLOC_FAILURE, return exit_code);
    CHECK(exit_code, reference_fbuffer == NULL, TL_ALLOC_FAILURE, return exit_code);
    for (size_t i = 0; i < VIDEO_FPS; ++i) {
        size_t bytes_read = fread(frame_buffer, sizeof(uint8_t), bytes_expected_per_frame, pipe);
        CHECK(
            exit_code, (bytes_read != bytes_expected_per_frame) && (!feof(pipe) || bytes_read != 0),
            TL_PIPE_PROCESS_FAILURE, goto epilogue
        );
        AcquireSRWLockShared(&thdata->pstate->srw);
        const size_t state_serial = thdata->pstate->current_serial;
        ReleaseSRWLockShared(&thdata->pstate->srw);
        CHECK(exit_code, state_serial != serial_at_call, TL_OUTDATED_SERIAL, goto epilogue);
        char    *frame = NULL;
        TRY(exit_code,
            frame_encode(
                frame_buffer, bytes_expected_per_frame, default_charset,
                i == 0 ? NULL : reference_fbuffer, &frame
            ),
            goto epilogue);
        TRY(exit_code, frame_compress(frame, &buffer[i]), goto epilogue);
        memcpy(reference_fbuffer, frame_buffer, bytes_expected_per_frame);
        frames_processed += 1;
        if (feof(pipe)) {
            break;
        }
    }
    if (frames_processed != VIDEO_FPS) {
        // The convention is to treat NULL pointers as errors, therefore we
        // need to allocate a filler frame with no bytes to differentiate between
        // actual NULL frames vs. empty frames.
        for (size_t i = 0; i < VIDEO_FPS - frames_processed; ++i) {
            buffer[i + frames_processed] = malloc(sizeof(""));
            CHECK(exit_code, buffer[i + frames_processed] == NULL, TL_ALLOC_FAILURE, goto epilogue);
            *buffer[i + frames_processed] = '\0';
        }
    }
epilogue:
    if (pipe != NULL) {
        _pclose(pipe);
        pipe = NULL;
    }
    free(reference_fbuffer);
    free(frame_buffer);
    return exit_code;
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
                serial, worker_thdata->thdata->pstate,
                clock_start + (worker_thdata->wthread_id == WORKER_A1 ? 0.0 : 1.0), media_path,
                worker_thdata->thdata->mtdta->streams_mask, target_buffer
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
                serial, worker_thdata->thdata,
                clock_start + (worker_thdata->wthread_id == WORKER_V1 ? 0.0 : 1.0), media_path,
                worker_thdata->thdata->mtdta->streams_mask, target_buffer, default_charset
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

tl_result frame_encode(
    uint8_t *frame_start,
    size_t   frame_size,
    bool     default_charset,
    uint8_t *keyframe,
    char   **frame
) {
    return TL_SUCCESS;
}


tl_result frame_compress(
    char* frame,
    char** compressed_out
) {
    return TL_SUCCESS;
}