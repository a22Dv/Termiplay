#include "tl_app.h"

tl_result app_exec(WCHAR *media_path) {
    tl_result exit_code = TL_SUCCESS;
    WCHAR    *media_rpath = NULL;
    metadata *media_mtdata = NULL;
    uint8_t   stream_count = 0;

    uint8_t       active_threads = 0;
    HANDLE        thread_handles[WORKER_THREAD_COUNT] = {0, 0, 0};
    thread_data **thdata = NULL;
    player_state *plstate = NULL;
    char        **video_buffer1 = NULL;
    char        **video_buffer2 = NULL;
    int16_t      *audio_buffer1 = NULL;
    int16_t      *audio_buffer2 = NULL;

    TRY(exit_code, is_valid_file(media_path, &media_rpath), goto epilogue);
    TRY(exit_code, get_stream_count(media_rpath, &stream_count), goto epilogue);
    TRY(exit_code, get_metadata(media_rpath, stream_count, &media_mtdata), goto epilogue);
    TRY(exit_code, create_player_state(&plstate), goto epilogue);

    // Audio present.
    if (stream_count & 0x01) {
        audio_buffer1 = malloc(sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);
        audio_buffer2 = malloc(sizeof(int16_t) * CHANNEL_COUNT * AUDIO_SAMPLE_RATE);
        CHECK(exit_code, audio_buffer1 == NULL, TL_ALLOC_FAILURE, goto epilogue);
        CHECK(exit_code, audio_buffer2 == NULL, TL_ALLOC_FAILURE, goto epilogue);
    }

    // Video present.
    if (stream_count >> 4) {
        video_buffer1 = malloc(sizeof(char *) * VIDEO_FPS);
        video_buffer2 = malloc(sizeof(char *) * VIDEO_FPS);
        CHECK(exit_code, video_buffer1 == NULL, TL_ALLOC_FAILURE, goto epilogue);
        CHECK(exit_code, video_buffer2 == NULL, TL_ALLOC_FAILURE, goto epilogue);
    }

    // Create/allocate thread data array.
    TRY(exit_code,
        create_thread_data(
            stream_count, audio_buffer1, audio_buffer2, video_buffer1, video_buffer2, plstate,
            media_mtdata, &thdata
        ),
        goto epilogue);

    // Start threads.
    for (size_t i = 0; i < WORKER_THREAD_COUNT; ++i) {
        thread_handles[i] =
            (HANDLE)_beginthreadex(NULL, 0, thread_dispatcher, (void *)thdata[i], 0, NULL);
        CHECK(exit_code, thread_handles[i] == 0, TL_THREAD_INIT_FAILURE, goto epilogue);
        active_threads += 1;
    }

    // Main loop.
    while (true) {
        // TODO.
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
    
    free(plstate);
    free(thdata); // Has internals with internals.
    free(media_rpath);
    free(media_mtdata);
    free(video_buffer1);
    free(video_buffer2);
    free(audio_buffer1);
    free(audio_buffer2);
    return exit_code;
}

tl_result proc_thread_exec(thread_data *thdata) {}

tl_result audio_thread_exec(thread_data *thdata) {}

tl_result video_thread_exec(thread_data *thdata) {}

unsigned int __stdcall thread_dispatcher(void *data) {
    tl_result (*dispatch[WORKER_THREAD_COUNT])(thread_data *) = {
        audio_thread_exec, video_thread_exec, proc_thread_exec
    };
    thread_data *data_arg = (thread_data *)data;
    tl_result    exec_code = dispatch[data_arg->thread_id](data_arg);
    return exec_code;
}
