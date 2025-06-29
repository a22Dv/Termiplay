#include "tl_utils.h"

tl_result is_valid_file(
    const WCHAR *media_path,
    WCHAR      **resolved_path
) {
    tl_result exit_code = TL_SUCCESS;
    CHECK(exit_code, media_path == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, resolved_path == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, *resolved_path != NULL, TL_OVERWRITE, return exit_code);

    // Resolve path.
    WCHAR *rpath_buf = malloc(sizeof(WCHAR) * WCHAR_PATH_MAX);
    CHECK(exit_code, rpath_buf == NULL, TL_ALLOC_FAILURE, return exit_code);
    DWORD ret = GetFullPathNameW(media_path, WCHAR_PATH_MAX, rpath_buf, NULL);
    CHECK(exit_code, ret >= WCHAR_PATH_MAX || ret == 0, TL_INVALID_PATH, goto epilogue);

    // Check for existence.
    DWORD attributes = GetFileAttributesW(media_path);
    if (attributes == 0xFFFFFFFF) {
        switch (GetLastError()) {
        case ERROR_FILE_NOT_FOUND:
            exit_code = TL_FILE_NOT_FOUND;
            goto epilogue;
        case ERROR_PATH_NOT_FOUND:
            exit_code = TL_FILE_NOT_FOUND;
            goto epilogue;
        case ERROR_ACCESS_DENIED:
            exit_code = TL_INSUFFICIENT_PERMISSIONS;
            goto epilogue;
        }
    }
    WCHAR *temp_block = realloc(rpath_buf, (ret + 1) * sizeof(WCHAR));
    CHECK(exit_code, temp_block == NULL, TL_ALLOC_FAILURE, goto epilogue);
    *resolved_path = temp_block;
epilogue:
    if (exit_code != TL_SUCCESS) {
        free(rpath_buf);
    }
    return exit_code;
}

tl_result get_stream_count(
    const WCHAR *media_path,
    uint8_t     *streams
) {
    tl_result exit_code = TL_SUCCESS;
    CHECK(exit_code, media_path == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, streams == NULL, TL_NULL_ARGUMENT, return exit_code);

    WCHAR cmd_buffer[BUFFER_SIZE];
    char  rbuf[BUFFER_SIZE];
    int   ret = swprintf(
        cmd_buffer, BUFFER_SIZE,
        L"ffprobe -v error -show_entries stream=codec_type -of default=noprint_wrappers=1:nokey=1 "
        L"\"%ls\"",
        media_path
    );
    CHECK(exit_code, ret < 0 || ret >= BUFFER_SIZE, TL_FORMAT_FAILURE, return exit_code);
    FILE *pipe = NULL;
    pipe = _wpopen(cmd_buffer, L"r");
    CHECK(exit_code, pipe == NULL, TL_PIPE_OPEN_FAILURE, return exit_code);
    size_t byte_offset = 0;
    while (fgets(rbuf + byte_offset, BUFFER_SIZE - byte_offset, pipe)) {
        size_t line_len = strlen(rbuf + byte_offset);
        byte_offset += line_len;
        rbuf[byte_offset - 1] = '\0';
    }
    int exit_pipe = _pclose(pipe);
    CHECK(exit_code, exit_pipe != 0, TL_PIPE_PROCESS_FAILURE, return exit_code);

    uint8_t vstreams = 0;
    uint8_t astreams = 0;
    for (size_t offset = 0; *(rbuf + offset) != '\0'; offset += (strlen(rbuf + offset) + 1)) {
        if (strcmp("video", rbuf + offset) == 0) {
            vstreams += 1;
            *streams |= 0x01;
        } else if ((strcmp("audio", rbuf + offset) == 0)) {
            astreams += 1;
            *streams |= 0x10;
        }
    }
    CHECK(exit_code, vstreams >= 2 || astreams >= 2, TL_INVALID_FILE, return exit_code);
    return exit_code;
}

tl_result get_metadata(
    const WCHAR  *media_path,
    const uint8_t streams,
    metadata    **mtptr
) {
    tl_result exit_code = TL_SUCCESS;
    CHECK(exit_code, media_path == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, mtptr == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, *mtptr != NULL, TL_OVERWRITE, return exit_code);

    metadata *mt = malloc(sizeof(metadata));
    CHECK(exit_code, mt == NULL, TL_ALLOC_FAILURE, return exit_code);

    FILE  *pipe = NULL;
    WCHAR  cmd_buffer[BUFFER_SIZE];
    char   rbuf[BUFFER_SIZE];
    double duration = 0.0;

    int ret = swprintf(
        cmd_buffer, BUFFER_SIZE,
        L"ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "
        L"\"%ls\"",
        media_path
    );
    CHECK(exit_code, ret < 0 || ret >= BUFFER_SIZE, TL_FORMAT_FAILURE, goto epilogue);

    pipe = _wpopen(cmd_buffer, L"r");
    CHECK(exit_code, pipe == NULL, TL_PIPE_OPEN_FAILURE, goto epilogue);
    ret = fscanf(pipe, "%lf", &duration);
    int exit_pipe = _pclose(pipe);
    CHECK(exit_code, ret != 1, TL_FORMAT_FAILURE, goto epilogue);
    CHECK(exit_code, exit_pipe != 0, TL_PIPE_PROCESS_FAILURE, goto epilogue);

    uint32_t num = 0;
    uint32_t denum = 0;
    uint16_t height = 0;
    uint16_t width = 0;
    uint8_t  fps = 0;

    // Audio only.
    if (!(streams >> 4)) {
        mt->duration = duration;
        mt->height = height;
        mt->width = width;
        mt->fps = fps;
        *mtptr = mt;
        goto epilogue;
    }

    ret = swprintf(
        cmd_buffer, BUFFER_SIZE,
        L"ffprobe -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate -of "
        L"default=noprint_wrappers=1:nokey=1 "
        L"\"%ls\"",
        media_path
    );
    CHECK(exit_code, ret < 0 || ret >= BUFFER_SIZE, TL_FORMAT_FAILURE, goto epilogue);

    pipe = _wpopen(cmd_buffer, L"r");
    CHECK(exit_code, pipe == NULL, TL_PIPE_OPEN_FAILURE, goto epilogue);
    ret = fscanf(pipe, "%hu %hu %u/%u", &width, &height, &num, &denum);
    exit_pipe = _pclose(pipe);
    CHECK(exit_code, exit_pipe != 0, TL_PIPE_PROCESS_FAILURE, goto epilogue);
    CHECK(exit_code, ret != 4, TL_FORMAT_FAILURE, goto epilogue);

    fps = (uint8_t)floor((double)num / denum);
    mt->duration = duration;
    mt->height = height;
    mt->width = width;
    mt->fps = fps;
    *mtptr = mt;

epilogue:
    if (exit_code != TL_SUCCESS) {
        free(mt);
    }
    return exit_code;
}

tl_result create_thread_data(
    uint8_t       streams,
    int16_t      *abuf1,
    int16_t      *abuf2,
    char        **vbuf1,
    char        **vbuf2,
    player_state *pstate,
    metadata     *mtdta,
    thread_data ***thptr
) {
    tl_result exit_code = TL_SUCCESS;
    CHECK(exit_code, thptr == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, *thptr != NULL, TL_OVERWRITE, return exit_code);
    CHECK(exit_code, pstate == NULL, TL_NULL_ARGUMENT, return exit_code);

    bool has_audio = streams & 0x01;
    bool has_video = (streams >> 4) & 0x01;
    CHECK(exit_code, !has_audio && !has_video, TL_INVALID_ARGUMENT, return exit_code);

    thread_data **data_arr = malloc(sizeof(thread_data *) * WORKER_THREAD_COUNT);
    CHECK(exit_code, data_arr == NULL, TL_ALLOC_FAILURE, return exit_code);
    memset(data_arr, 0, sizeof(thread_data *) * WORKER_THREAD_COUNT);
    
    thread_data *proc_thdata = NULL; 
    thread_data *video_thdata = NULL;
    thread_data *audio_thdata = NULL;

    proc_thdata = malloc(sizeof(thread_data));
    CHECK(exit_code, proc_thdata == NULL, TL_ALLOC_FAILURE, goto epilogue);

    proc_thdata->audio_buffer1 = abuf1;
    proc_thdata->audio_buffer2 = abuf2;
    proc_thdata->pstate = pstate;
    proc_thdata->thread_id = PROC_THREAD_ID;
    proc_thdata->video_buffer1 = vbuf1;
    proc_thdata->video_buffer2 = vbuf2;
    proc_thdata->mtdta = mtdta;
    data_arr[PROC_THREAD_ID] = proc_thdata;

    if (has_audio) {
        CHECK(exit_code, abuf1 == NULL, TL_NULL_ARGUMENT, goto epilogue);
        CHECK(exit_code, abuf2 == NULL, TL_NULL_ARGUMENT, goto epilogue);

        audio_thdata = malloc(sizeof(thread_data));
        CHECK(exit_code, audio_thdata == NULL, TL_ALLOC_FAILURE, goto epilogue);

        audio_thdata->audio_buffer1 = abuf1;
        audio_thdata->audio_buffer2 = abuf2;
        audio_thdata->pstate = pstate;
        audio_thdata->thread_id = AUDIO_THREAD_ID;
        audio_thdata->video_buffer1 = vbuf1;
        audio_thdata->video_buffer2 = vbuf2;
        audio_thdata->mtdta = mtdta;
        data_arr[AUDIO_THREAD_ID] = audio_thdata;
    }
    if (has_video) {
        CHECK(exit_code, vbuf1 == NULL, TL_NULL_ARGUMENT, goto epilogue);
        CHECK(exit_code, vbuf2 == NULL, TL_NULL_ARGUMENT, goto epilogue);

        video_thdata = malloc(sizeof(thread_data));
        CHECK(exit_code, video_thdata == NULL, TL_ALLOC_FAILURE, goto epilogue);

        video_thdata->audio_buffer1 = abuf1;
        video_thdata->audio_buffer2 = abuf2;
        video_thdata->pstate = pstate;
        video_thdata->thread_id = VIDEO_THREAD_ID;
        video_thdata->video_buffer1 = vbuf1;
        video_thdata->video_buffer2 = vbuf2;
        video_thdata->mtdta = mtdta;
        data_arr[VIDEO_THREAD_ID] = video_thdata;
    }
    *thptr = data_arr;
epilogue:
    if (exit_code != TL_SUCCESS) {
        for (size_t i = 0; i < WORKER_THREAD_COUNT; ++i) {
            free(data_arr[i]);
        }
        free(data_arr);
    }
    return exit_code;
}

tl_result create_player_state(player_state **pl_state_ptr) {
    tl_result exit_code = TL_SUCCESS;
    CHECK(exit_code, pl_state_ptr == NULL, TL_NULL_ARGUMENT, return exit_code);
    CHECK(exit_code, *pl_state_ptr != NULL, TL_OVERWRITE, return exit_code);

    player_state* pstate = malloc(sizeof(player_state));
    CHECK(exit_code, pstate == NULL, TL_ALLOC_FAILURE, return exit_code);

    pstate->shutdown = false;
    pstate->looping = false;
    pstate->main_clock = 0.0;
    pstate->muted = false;
    pstate->playback = false;
    pstate->seek_idx = 0;
    pstate->seeking = false;
    pstate->volume = 0.5;
    InitializeSRWLock(&pstate->srw);

    *pl_state_ptr = pstate;
    return exit_code;
}
