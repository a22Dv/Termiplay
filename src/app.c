#include "tl_app.h"
#define POLLING_RATE_MS 50
#define SEEK_SPEEDS 8

tl_result app_exec(WCHAR *media_path) {
    tl_result exit_code = TL_SUCCESS;
    WCHAR    *media_rpath = NULL;
    metadata *media_mtdata = NULL;
    uint8_t   stream_count = 0;

    uint8_t               active_threads = 0;
    HANDLE                thread_handles[WORKER_THREAD_COUNT] = {0, 0, 0};
    thread_data         **thdata = NULL;
    player_state         *plstate = NULL;
    char                **video_buffer1 = NULL;
    char                **video_buffer2 = NULL;
    int16_t              *audio_buffer1 = NULL;
    int16_t              *audio_buffer2 = NULL;
    static const uint16_t seek_multiples[SEEK_SPEEDS] = {5, 15, 30, 60, 120, 240, 480, 960};

    TRY(exit_code, is_valid_file(media_path, &media_rpath), goto epilogue);
    TRY(exit_code, get_stream_count(media_rpath, &stream_count), goto epilogue);
    TRY(exit_code, get_metadata(media_rpath, stream_count, &media_mtdata), goto epilogue);
    TRY(exit_code, create_player_state(&plstate), goto epilogue);

    // Audio present.
    if (stream_count & 0x01) {
        audio_buffer1 = malloc(sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);
        audio_buffer2 = malloc(sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);

        CHECK(exit_code, audio_buffer1 == NULL, TL_ALLOC_FAILURE, goto epilogue);
        memset(audio_buffer1, 0, sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);

        CHECK(exit_code, audio_buffer2 == NULL, TL_ALLOC_FAILURE, goto epilogue);
        memset(audio_buffer2, 0, sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);
    }

    // Video present.
    if (stream_count >> 4) {
        video_buffer1 = malloc(sizeof(char *) * VIDEO_FPS);
        video_buffer2 = malloc(sizeof(char *) * VIDEO_FPS);

        CHECK(exit_code, video_buffer1 == NULL, TL_ALLOC_FAILURE, goto epilogue);
        memset(video_buffer1, 0, sizeof(char *) * VIDEO_FPS);

        CHECK(exit_code, video_buffer2 == NULL, TL_ALLOC_FAILURE, goto epilogue);
        memset(video_buffer2, 0, sizeof(char *) * VIDEO_FPS);
    }

    // Create/allocate thread data array.
    TRY(exit_code,
        create_thread_data(
            stream_count, audio_buffer1, audio_buffer2, video_buffer1, video_buffer2, plstate,
            media_mtdata, &thdata
        ),
        goto epilogue);

    // Start threads conditionally.
    for (size_t i = 0; i < WORKER_THREAD_COUNT; ++i) {
        if (i == AUDIO_THREAD_ID && stream_count & 0x01 || i == PROC_THREAD_ID ||
            i == VIDEO_THREAD_ID && stream_count >> 4) {
            thread_handles[i] =
                (HANDLE)_beginthreadex(NULL, 0, thread_dispatcher, (void *)thdata[i], 0, NULL);
            CHECK(exit_code, thread_handles[i] == 0, TL_THREAD_INIT_FAILURE, goto epilogue);
            active_threads += 1;
        }
    }

    system("cls");
    // Main loop.
    while (true) {
        AcquireSRWLockShared(&plstate->srw);
        bool shutdown_status = plstate->shutdown;
        ReleaseSRWLockShared(&plstate->srw);
        if (shutdown_status) {
            break;
        }
        uint8_t key_code = 0;
        TRY(exit_code, poll_input(&key_code), goto epilogue);
        TRY(exit_code, proc_input(key_code, seek_multiples, media_mtdata, plstate), goto epilogue);
#ifdef DEBUG
        printf("\x1b[H");
        printf(
            "SHUTDOWN: %s\nPLAYBACK: %s\nLOOPING: %s\nSEEKING: %s\nMUTED: %s\nDEFAULT_CHARSET: "
            "%s\nVOLUME: %lf\nMAIN_CLOCK: %lf\nSEEK_VAR: %lf\nSEEK_IDX: %llu\n",
            plstate->shutdown ? " TRUE" : "FALSE", plstate->playback ? " TRUE" : "FALSE",
            plstate->looping ? " TRUE" : "FALSE", plstate->seeking ? " TRUE" : "FALSE",
            plstate->muted ? " TRUE" : "FALSE", plstate->default_charset ? " TRUE" : "FALSE",
            plstate->volume, plstate->main_clock, plstate->seek_variable, plstate->seek_idx
        );
#endif
        Sleep(POLLING_RATE_MS);
    }

epilogue:
    if (active_threads != 0) {
        AcquireSRWLockExclusive(&plstate->srw);
        plstate->shutdown = true;
        ReleaseSRWLockExclusive(&plstate->srw);
        WaitForMultipleObjects(active_threads, thread_handles, true, INFINITE);
        for (size_t i = 0; i < WORKER_THREAD_COUNT; ++i) {
            if (thread_handles[i] != NULL) {
                CloseHandle(thread_handles[i]);
                thread_handles[i] = NULL;
            }
        }
    }
    if (thdata != NULL) {
        for (size_t i = 0; i < WORKER_THREAD_COUNT; ++i) {
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
    if (!kbhit()) {
        *key_code = 0;
        return TL_SUCCESS;
    }
    uint8_t key = _getch();
    if (key != EXTENDED) {
        *key_code = key;
        while (kbhit()) {
            _getch();
        }
        return TL_SUCCESS;
    }
    uint8_t ext_key = _getch();
    *key_code = ext_key;
    while (kbhit()) {
        _getch();
    }
    return TL_SUCCESS;
}

tl_result proc_input(
    const uint8_t   key_code,
    const uint16_t *seek_multiples,
    const metadata *mtdta,
    player_state   *plstate
) {
    AcquireSRWLockExclusive(&plstate->srw);
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
        plstate->seeking = true;
        plstate->seek_variable += (double)POLLING_RATE_MS / 1000;
        size_t new_idx = (size_t)floor(plstate->seek_variable);
        plstate->seek_idx = new_idx < SEEK_SPEEDS - 1 ? new_idx : SEEK_SPEEDS - 1;
        double new_clock = plstate->main_clock - seek_multiples[plstate->seek_idx];
        plstate->main_clock = new_clock > 0.0 ? new_clock : 0.0;
        break;
    }
    case ARROW_RIGHT: {
        plstate->seeking = true;
        plstate->seek_variable += (double)POLLING_RATE_MS / 1000;
        size_t new_idx = (size_t)floor(plstate->seek_variable);
        plstate->seek_idx = new_idx < SEEK_SPEEDS - 1 ? new_idx : SEEK_SPEEDS - 1;
        double new_clock = plstate->main_clock + seek_multiples[plstate->seek_idx];
        plstate->main_clock = new_clock < mtdta->duration ? new_clock : mtdta->duration;
        break;
    }
    default: {
        plstate->seeking = false;
        plstate->seek_idx = 0;
        plstate->seek_variable = 0.0;
        break;
    }
    }
    ReleaseSRWLockExclusive(&plstate->srw);
    return TL_SUCCESS;
}

tl_result proc_thread_exec(thread_data *thdata) {
    while (true) {
        AcquireSRWLockShared(&thdata->pstate->srw);
        bool shutdown_status = thdata->pstate->shutdown;
        ReleaseSRWLockShared(&thdata->pstate->srw);

        if (shutdown_status) {
            break;
        }
        Sleep(1);
    }
epilogue:
    return TL_SUCCESS;
}

tl_result audio_thread_exec(thread_data *thdata) {
    while (true) {
        AcquireSRWLockShared(&thdata->pstate->srw);
        bool shutdown_status = thdata->pstate->shutdown;
        ReleaseSRWLockShared(&thdata->pstate->srw);
        
        if (shutdown_status) {
            break;
        }
        Sleep(1);
    }
epilogue:
    return TL_SUCCESS;
}

tl_result video_thread_exec(thread_data *thdata) {
    while (true) {
        AcquireSRWLockShared(&thdata->pstate->srw);
        bool shutdown_status = thdata->pstate->shutdown;
        ReleaseSRWLockShared(&thdata->pstate->srw);

        if (shutdown_status) {
            break;
        }

        Sleep(1);
    }
epilogue:
    return TL_SUCCESS;
}

unsigned int __stdcall thread_dispatcher(void *data) {
    tl_result (*dispatch[WORKER_THREAD_COUNT])(thread_data *) = {
        audio_thread_exec, video_thread_exec, proc_thread_exec
    };
    thread_data *data_arg = (thread_data *)data;
    tl_result    exec_code = dispatch[data_arg->thread_id](data_arg);
    return exec_code;
}
