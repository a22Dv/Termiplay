#include "tl_app.h"

tl_result app_exec(WCHAR *media_path) {
    tl_result             exit_code = TL_SUCCESS;
    WCHAR                *media_rpath = NULL;
    metadata             *media_mtdata = NULL;
    uint8_t               stream_count = 0;
    uint8_t               active_threads = 0;
    HANDLE               *thread_handles = NULL;
    thread_data         **thdata = NULL;
    player_state         *plstate = NULL;
    char                **video_buffer1 = NULL;
    char                **video_buffer2 = NULL;
    int16_t              *audio_buffer1 = NULL;
    int16_t              *audio_buffer2 = NULL;
    static const uint16_t seek_multiples[SEEK_SPEEDS] = {1, 5, 15, 30, 60, 120, 240, 480};

    CHECK(exit_code, media_path == NULL, TL_NULL_ARGUMENT, goto epilogue);
    TRY(exit_code, is_valid_file(media_path, &media_rpath), goto epilogue);
    TRY(exit_code, get_stream_count(media_rpath, &stream_count), goto epilogue);
    TRY(exit_code, get_metadata(media_rpath, stream_count, &media_mtdata), goto epilogue);
    TRY(exit_code, create_player_state(&plstate), goto epilogue);

    const size_t thread_count = WORKER_THREAD_COUNT - (stream_count & VIDEO_PRESENT ? 0 : 1);

    // Audio is always present. Will be fed silence in absence. Required for main clock.
    audio_buffer1 = malloc(sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);
    audio_buffer2 = malloc(sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);
    CHECK(exit_code, audio_buffer1 == NULL, TL_ALLOC_FAILURE, goto epilogue);
    CHECK(exit_code, audio_buffer2 == NULL, TL_ALLOC_FAILURE, goto epilogue);
    memset(audio_buffer1, 0, sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);
    memset(audio_buffer2, 0, sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);

    // Video present.
    if (stream_count & VIDEO_PRESENT) {
        video_buffer1 = malloc(sizeof(char *) * VIDEO_FPS);
        video_buffer2 = malloc(sizeof(char *) * VIDEO_FPS);

        CHECK(exit_code, video_buffer1 == NULL, TL_ALLOC_FAILURE, goto epilogue);
        memset(video_buffer1, 0, sizeof(char *) * VIDEO_FPS);

        CHECK(exit_code, video_buffer2 == NULL, TL_ALLOC_FAILURE, goto epilogue);
        memset(video_buffer2, 0, sizeof(char *) * VIDEO_FPS);
    }

    thread_handles = malloc(sizeof(HANDLE) * thread_count);
    CHECK(exit_code, thread_handles == NULL, TL_ALLOC_FAILURE, goto epilogue);
    memset(thread_handles, 0, sizeof(HANDLE) * thread_count);

    // Create/allocate thread data array.
    TRY(exit_code,
        create_thread_data(
            stream_count, audio_buffer1, audio_buffer2, video_buffer1, video_buffer2, plstate,
            media_mtdata, &thdata
        ),
        goto epilogue);

    // Start threads conditionally.
    for (size_t i = 0; i < thread_count; ++i) {
        thread_handles[i] =
            (HANDLE)_beginthreadex(NULL, 0, thread_dispatcher, (void *)thdata[i], 0, NULL);
        CHECK(exit_code, thread_handles[i] == NULL, TL_THREAD_INIT_FAILURE, goto epilogue);
        active_threads += 1;
    }

    bool                       prev_seeking = false;
    CONSOLE_SCREEN_BUFFER_INFO prev_csbi;
    CHECK(
        exit_code, !GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &prev_csbi),
        TL_CONSOLE_ERROR, goto epilogue
    );
    CONSOLE_SCREEN_BUFFER_INFO curr_csbi;
    CHECK(
        exit_code, !GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &curr_csbi),
        TL_CONSOLE_ERROR, goto epilogue
    );

    // Main loop.
    while (true) {
        AcquireSRWLockShared(&plstate->control_srw);
        AcquireSRWLockShared(&plstate->clock_srw);
        bool shutdown_status = plstate->shutdown;
        bool cur_playback_state = plstate->playback;
        bool cur_looping_state = plstate->looping;
        double cur_clock = plstate->main_clock;
        ReleaseSRWLockShared(&plstate->clock_srw);
        ReleaseSRWLockShared(&plstate->control_srw);
        if (shutdown_status) {
            break;
        }

        DWORD waitResult = WaitForMultipleObjects((DWORD)thread_count, thread_handles, FALSE, 0);
        if (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + thread_count) {
        }
        // Check for premature thread shutdowns.
        for (size_t i = 0; i < thread_count; ++i) {
            if (WaitForSingleObject(thread_handles[i], 0) != WAIT_TIMEOUT) {
                tl_result terminated_code = TL_GENERIC_ERROR;
                DWORD     w_exit_code = 0;
                CHECK(
                    terminated_code, !GetExitCodeThread(thread_handles[i], &w_exit_code),
                    TL_GENERIC_ERROR, goto epilogue
                );
                exit_code = (tl_result)w_exit_code;
                goto epilogue;
            }
        }

        uint8_t key_code = 0;
        TRY(exit_code, poll_input(&key_code), goto epilogue);
        TRY(exit_code, proc_input(key_code, seek_multiples, media_mtdata, plstate), goto epilogue);
        CHECK(
            exit_code, !GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &curr_csbi),
            TL_CONSOLE_ERROR, goto epilogue
        );

        AcquireSRWLockExclusive(&plstate->control_srw);
        // Looping.
        if (cur_looping_state && cur_clock >= media_mtdata->duration) {
            plstate->seeking = true;
            AcquireSRWLockExclusive(&plstate->clock_srw);
            plstate->main_clock = 0.0;
            ReleaseSRWLockExclusive(&plstate->clock_srw);
        }
        // Console buffer resize invalidation.
        if (prev_csbi.srWindow.Top != curr_csbi.srWindow.Top ||
            prev_csbi.srWindow.Left != curr_csbi.srWindow.Left ||
            prev_csbi.srWindow.Bottom != curr_csbi.srWindow.Bottom ||
            prev_csbi.srWindow.Right != curr_csbi.srWindow.Right) {
            plstate->seeking = true;
            prev_csbi = curr_csbi;
        }
        // Invalidation. Increase serial.
        if (plstate->seeking && !prev_seeking) {
            AcquireSRWLockExclusive(&plstate->buffer_serial_srw);
            plstate->current_serial += 1;
            ReleaseSRWLockExclusive(&plstate->buffer_serial_srw);
        }
        prev_seeking = plstate->seeking;
        ReleaseSRWLockExclusive(&plstate->control_srw);
#ifdef DEBUG
        state_print(plstate);
#endif
        Sleep(POLLING_RATE_MS);
    }
epilogue:
    if (active_threads != 0) {
        AcquireSRWLockExclusive(&plstate->control_srw);
        plstate->shutdown = true;
        ReleaseSRWLockExclusive(&plstate->control_srw);
        WaitForMultipleObjects(active_threads, thread_handles, true, INFINITE);
        for (size_t i = 0; i < active_threads; ++i) {
            if (thread_handles[i] != NULL) {
                CloseHandle(thread_handles[i]);
                thread_handles[i] = NULL;
            }
        }
        free(thread_handles);
    }
    if (thdata != NULL) {
        for (size_t i = 0; i < WORKER_THREAD_COUNT - (stream_count & VIDEO_PRESENT ? 0 : 1); ++i) {

            // Structs themselves are free-ed, internals are just pointers to declared
            // variables declared in this scope and free-ed in the next block explicitly.
            free(thdata[i]);
            thdata[i] = NULL;
        }
        free(thdata);
    }
    free(plstate);
    free(media_rpath);
    free(media_mtdata);
    if (video_buffer1 != NULL) {
        for (size_t i = 0; i < VIDEO_FPS; ++i) {
            free(video_buffer1[i]);
        }
        free(video_buffer1);
    }
    if (video_buffer2 != NULL) {
        for (size_t i = 0; i < VIDEO_FPS; ++i) {
            free(video_buffer2[i]);
        }
        free(video_buffer2);
    }
    free(audio_buffer1);
    free(audio_buffer2);
    return exit_code;
}

tl_result poll_input(uint8_t *key_code) {
    tl_result exit_code = TL_SUCCESS;
    CHECK(exit_code, key_code == NULL, TL_NULL_ARGUMENT, return exit_code);
    if (!_kbhit()) {
        *key_code = 0;
        return exit_code;
    }
    uint8_t key = (uint8_t)_getch();
    if (key != EXTENDED) {
        *key_code = key;
        while (_kbhit()) {
            _getch();
        }
        return exit_code;
    }
    uint8_t ext_key = (uint8_t)_getch();
    *key_code = ext_key;
    while (_kbhit()) {
        _getch();
    }
    return exit_code;
}

tl_result proc_input(
    const uint8_t   key_code,
    const uint16_t *seek_multiples,
    const metadata *mtdta,
    player_state   *plstate
) {
    AcquireSRWLockExclusive(&plstate->control_srw);
    switch (key_code) {
    case LOOP: {
        plstate->looping = !plstate->looping;
        break;
    }
    case MUTE: {
        plstate->muted = !plstate->muted;
        break;
    }
    case TOGGLE_CHARSET: {
        plstate->default_charset = !plstate->default_charset;
        plstate->seeking = true;
        break;
    }
    case TOGGLE_PLAYBACK: {
        plstate->playback = !plstate->playback;
        break;
    }
    case QUIT: {
        plstate->shutdown = true;
        break;
    }
    case ARROW_UP: {
        plstate->volume = 1.0 > plstate->volume + 0.05 ? plstate->volume + 0.05 : 1.0;
        break;
    }
    case ARROW_DOWN: {
        plstate->volume = 0.0 < plstate->volume - 0.05 ? plstate->volume - 0.05 : 0.0;
        break;
    }
    case ARROW_LEFT: {
        AcquireSRWLockExclusive(&plstate->clock_srw);
        plstate->seeking = true;
        plstate->seek_variable += (double)POLLING_RATE_MS / 1000;
        size_t new_idx = (size_t)floor(plstate->seek_variable);
        plstate->seek_idx = new_idx < SEEK_SPEEDS - 1 ? new_idx : SEEK_SPEEDS - 1;
        double new_clock = plstate->main_clock - seek_multiples[plstate->seek_idx];
        plstate->main_clock = new_clock > 0.0 ? new_clock : 0.0;
        ReleaseSRWLockExclusive(&plstate->clock_srw);
        break;
    }
    case ARROW_RIGHT: {
        AcquireSRWLockExclusive(&plstate->clock_srw);
        plstate->seeking = true;
        plstate->seek_variable += (double)POLLING_RATE_MS / 1000;
        size_t new_idx = (size_t)floor(plstate->seek_variable);
        plstate->seek_idx = new_idx < SEEK_SPEEDS - 1 ? new_idx : SEEK_SPEEDS - 1;
        double new_clock = plstate->main_clock + seek_multiples[plstate->seek_idx];
        plstate->main_clock = new_clock < mtdta->duration ? new_clock : mtdta->duration;
        ReleaseSRWLockExclusive(&plstate->clock_srw);
        break;
    }
    default: {
        plstate->seeking = false;
        plstate->seek_idx = 0;
        plstate->seek_variable = 0.0;
        break;
    }
    }
    ReleaseSRWLockExclusive(&plstate->control_srw);
    return TL_SUCCESS;
}

unsigned int __stdcall thread_dispatcher(void *data) {
    tl_result (*dispatch[WORKER_THREAD_COUNT])(thread_data *) = {
        audio_thread_exec, proc_thread_exec, video_thread_exec
    };
    thread_data *data_arg = (thread_data *)data;
    tl_result    exec_code = dispatch[data_arg->thread_id](data_arg);
    return exec_code;
}

unsigned int __stdcall helper_thread_dispatcher(void *data) {
    tl_result (*dispatch[2])(wthread_data *) = {audio_helper_thread_exec, video_helper_thread_exec};
    wthread_data *wthdta = (wthread_data *)data;
    tl_result     exec_code = dispatch[wthdta->wthread_id < 2 ? 0 : 1](wthdta);
    return exec_code;
}
