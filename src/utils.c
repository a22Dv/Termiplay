#include "tl_errors.h"
#include "tl_pch.h"
#include "tl_types.h"
#include "tl_utils.h"

void set_atomic_bool(
    atomic_bool_t *b,
    bool           value
) {
    if (b == NULL) {
        return;
    }
    _InterlockedExchange((volatile LONG *)b, value);
}

bool get_atomic_bool(atomic_bool_t *b) {
    if (b == NULL) {
        return false;
    }
    return (bool)_InterlockedOr((volatile LONG *)b, 0);
}

void flip_atomic_bool(atomic_bool_t *b) {
    if (b == NULL) {
        return;
    }
    _InterlockedXor((volatile LONG *)b, 1);
}

void set_atomic_double(
    atomic_double_t *dbl,
    double           value
) {
    if (dbl == NULL) {
        return;
    }
    union double_l64 dl64;
    dl64.d = value;
    _InterlockedExchange64(dbl, dl64.l64);
}

double get_atomic_double(atomic_double_t *dbl) {
    if (dbl == NULL) {
        return 0.0;
    }
    union double_l64 dl64;
    dl64.l64 = _InterlockedOr64(dbl, 0);
    return dl64.d;
}

void add_atomic_double(
    atomic_double_t *dbl,
    double           addend
) {
    if (dbl == NULL) {
        return;
    }
    union double_l64 dl64;
    union double_l64 dl64_r;
    dl64.l64 = _InterlockedOr64(dbl, 0);
    while (true) {
        LONG64 cmp = dl64.l64;
        dl64_r.d = dl64.d + addend;
        LONG64 v = _InterlockedCompareExchange64(dbl, dl64_r.l64, cmp);
        if (v != cmp) {
            dl64.l64 = v;
            continue;
        }
        break;
    }
}

void set_atomic_size_t(
    atomic_size_t *st,
    size_t         value
) {
    if (st == NULL) {
        return;
    }
    _InterlockedExchange64(st, value);
}

size_t get_atomic_size_t(atomic_size_t *st) {
    if (st == NULL) {
        return 0;
    }
    return (size_t)_InterlockedOr64(st, 0);
}

void add_atomic_size_t(
    atomic_size_t *st,
    size_t         addend
) {
    if (st == NULL) {
        return;
    }
    while (true) {
        LONG64 cmp = _InterlockedOr64(st, 0);
        LONG64 r = cmp + (LONG64)addend;
        if (cmp == _InterlockedCompareExchange64(st, r, cmp)) {
            break;
        }
    }
}

tl_result create_media_mtdta(
    const WCHAR        *media_path,
    const media_mtdta **out
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, media_path == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, out == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, *out != NULL, TL_ALREADY_INITIALIZED, return excv);

    media_mtdta *mtdta = NULL;
    mtdta = malloc(sizeof(media_mtdta));
    CHECK(excv, mtdta == NULL, TL_ALLOC_FAILURE, return excv);

    mtdta->media_path = NULL;
    mtdta->duration = 0.0;
    mtdta->height = 0;
    mtdta->width = 0;
    mtdta->video_present = false;
    mtdta->audio_present = false;

    FILE *cmd_pipe = NULL;
    WCHAR cmd[GLBUFFER_BSIZE];
    WCHAR proc_res[GBUFFER_BSIZE];

    size_t wpathn_len = (wcslen(media_path) + 1) * sizeof(WCHAR);
    mtdta->media_path = malloc(wpathn_len);
    CHECK(excv, mtdta->media_path == NULL, TL_ALLOC_FAILURE, goto epilogue);
    memcpy(mtdta->media_path, media_path, wpathn_len);

    const WCHAR *const cmds_tmpl[3] = {
        L"ffprobe -v quiet -show_entries format=duration -of csv=p=0:nk=1 \"%ls\"",
        L"ffprobe -v quiet -select_streams v:0 -show_entries stream=width,height -of "
        L"csv=p=0:nk=1 \"%ls\"",
        L"ffprobe -v quiet -select_streams a:0 -show_entries stream=index -of csv=p=0:nk=1 "
        L"\"%ls\""
    };

    int ret = 0;
    for (size_t idx = 0; idx < 3; ++idx) {
        proc_res[0] = '\0';
        ret = swprintf_s(cmd, GLBUFFER_BSIZE, cmds_tmpl[idx], media_path);
        CHECK(excv, ret == -1, TL_FORMAT_FAILURE, goto epilogue);
        cmd_pipe = _wpopen(cmd, L"r");
        CHECK(excv, cmd_pipe == NULL, TL_PIPE_CREATION_FAILURE, goto epilogue);
        fgetws(proc_res, GBUFFER_BSIZE, cmd_pipe);
        CHECK(excv, ferror(cmd_pipe), TL_PIPE_READ_FAILURE, goto epilogue);
        switch (idx) {
        case 0:
            ret = swscanf_s(proc_res, L"%lf", &mtdta->duration);
            break;
        case 1:
            ret = swscanf_s(proc_res, L"%zu,%zu", &mtdta->width, &mtdta->height);
            mtdta->video_present = ret == 2;
            break;
        case 2:
            mtdta->audio_present = wcslen(proc_res) > 0;
            break;
        }
        ret = _pclose(cmd_pipe);
        cmd_pipe = NULL;
        CHECK(excv, ret != 0, TL_PIPE_PROC_FAILURE, goto epilogue);
    }
    *out = mtdta;
epilogue:
    if (excv != TL_SUCCESS) {
        destroy_media_mtdta(&mtdta);
    }
    if (cmd_pipe) {
        _pclose(cmd_pipe);
        cmd_pipe = NULL;
    }
    return excv;
}

tl_result create_thread_data(
    player         *pl,
    const thread_id id,
    thread_data   **out
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, pl == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, id < 0 || id > 3, TL_INVALID_ARG, return excv);
    CHECK(excv, out == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, *out != NULL, TL_ALREADY_INITIALIZED, return excv);

    *out = malloc(sizeof(thread_data));
    CHECK(excv, *out == NULL, TL_ALLOC_FAILURE, return excv);
    (*out)->player = pl;
    (*out)->thread_id = id;
    return excv;
}

unsigned int _stdcall thread_dispatcher(void *data) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, data == NULL, TL_NULL_ARG, return excv);
    thread_data *thdata = (thread_data *)data;
    switch (thdata->thread_id) {
    case AUDIO_THREAD_ID:
        TRY(excv, acthread_exec(thdata), return excv);
        break;
    case AUDIO_PROD_THREAD_ID:
        TRY(excv, apthread_exec(thdata), return excv);
        break;
    case VIDEO_THREAD_ID:
        TRY(excv, vcthread_exec(thdata), return excv);
        break;
    case VIDEO_PROD_THREAD_ID:
        TRY(excv, vpthread_exec(thdata), return excv);
        break;
    }
    return excv;
}

tl_result create_player(
    const WCHAR *media_path,
    player     **out
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, out == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, *out != NULL, TL_ALREADY_INITIALIZED, return excv);

    player *pl = malloc(sizeof(player));
    CHECK(excv, pl == NULL, TL_ALLOC_FAILURE, return excv);

    pl->video_rbuffer = NULL;
    pl->audio_rbuffer = NULL;
    pl->gwpvbuffer = NULL;
    pl->gwcvbuffer = NULL;
    pl->th_hndles = NULL;
    pl->ev_hndles = NULL;
    pl->th_data = NULL;
    pl->media_mtdta = NULL;
    set_atomic_bool(&pl->shutdown, false);
    set_atomic_bool(&pl->playing, false);
    set_atomic_bool(&pl->looping, false);
    set_atomic_bool(&pl->invalidated, false);
    set_atomic_bool(&pl->muted, false);
    set_atomic_double(&pl->main_clock, 0.0);
    set_atomic_double(&pl->volume, 0.0);
    set_atomic_double(&pl->seek_speed, 0.0);
    set_atomic_size_t(&pl->serial, 0);
    set_atomic_size_t(&pl->aread_idx, 0);
    set_atomic_size_t(&pl->awrite_idx, 0);
    set_atomic_size_t(&pl->vwrite_idx, 0);
    set_atomic_size_t(&pl->vread_idx, 0);
    InitializeSRWLock(&pl->srw_mclock);
    pl->active_threads = 0;

    TRY(excv, create_media_mtdta(media_path, &pl->media_mtdta), goto epilogue);

    // Audio present regardless of presence in media file due to clock/time-keeping.
    int16_t *audio_rbuffer = calloc(ABUFFER_BSIZE / sizeof(int16_t), sizeof(int16_t));
    CHECK(excv, audio_rbuffer == NULL, TL_ALLOC_FAILURE, goto epilogue);
    pl->audio_rbuffer = audio_rbuffer;

    if (pl->media_mtdta->video_present) {
        con_frame **video_rbuffer =
            calloc(VBUFFER_BSIZE / sizeof(con_frame *), sizeof(con_frame *));
        char *gwpvbuffer = malloc(GWVBUFFER_BSIZE);
        char *gwcvbuffer = malloc(GWVBUFFER_BSIZE);
        CHECK(excv, video_rbuffer == NULL, TL_ALLOC_FAILURE, goto epilogue);
        CHECK(excv, gwpvbuffer == NULL, TL_ALLOC_FAILURE, goto epilogue);
        CHECK(excv, gwcvbuffer == NULL, TL_ALLOC_FAILURE, goto epilogue);

        pl->video_rbuffer = video_rbuffer;
        pl->gwpvbuffer = gwpvbuffer;
        pl->gwcvbuffer = gwcvbuffer;
    }

    static const ev_handles event_handles[MAX_EVENTS] = {
        AUDIO_PROD_EVENT_WAKE_HNDLE, VIDEO_PROD_EVENT_WAKE_HNDLE
    };
    static const th_handles thread_handles[MAX_THREADS] = {
        AUDIO_THREAD_HNDLE, AUDIO_PROD_THREAD_HNDLE, VIDEO_THREAD_HNDLE, VIDEO_PROD_THREAD_HNDLE
    };
    static const thread_id thread_ids[MAX_THREADS] = {
        AUDIO_THREAD_ID, AUDIO_PROD_THREAD_ID, VIDEO_THREAD_ID, VIDEO_PROD_THREAD_ID
    };
    pl->ev_hndles = calloc(MAX_EVENTS, sizeof(HANDLE));
    CHECK(excv, pl->ev_hndles == NULL, TL_ALLOC_FAILURE, goto epilogue);
    for (size_t i = 0; i < (pl->media_mtdta->video_present ? MAX_EVENTS : MAX_EVENTS / 2); ++i) {
        pl->ev_hndles[i] = CreateEventW(NULL, false, false, NULL);
        CHECK(excv, pl->ev_hndles[i] == INVALID_HANDLE_VALUE, TL_OS_ERR, goto epilogue);
    }

    pl->th_data = calloc(MAX_THREADS, sizeof(thread_data *));
    CHECK(excv, pl->th_data == NULL, TL_ALLOC_FAILURE, goto epilogue);
    pl->th_hndles = calloc(MAX_THREADS, sizeof(HANDLE));
    CHECK(excv, pl->th_hndles == NULL, TL_ALLOC_FAILURE, goto epilogue);
    for (size_t i = 0; i < (pl->media_mtdta->video_present ? MAX_THREADS : MAX_THREADS / 2); ++i) {
        TRY(excv, create_thread_data(pl, thread_ids[i], &pl->th_data[i]), goto epilogue);
        pl->th_hndles[thread_handles[i]] =
            (HANDLE)_beginthreadex(NULL, 0, thread_dispatcher, (void *)pl->th_data[i], 0, NULL);
        CHECK(excv, pl->th_hndles[i] == INVALID_HANDLE_VALUE, TL_OS_ERR, goto epilogue);
        pl->active_threads++;
    }
    *out = pl;
epilogue:
    if (excv != TL_SUCCESS) {
        destroy_player(&pl);
    }
    return excv;
}

tl_result copy_rawframe(
    raw_frame  *src,
    raw_frame **dst_out
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, src == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, dst_out == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, *dst_out != NULL, TL_ALREADY_INITIALIZED, return excv);

    *dst_out = malloc(sizeof(raw_frame));
    CHECK(excv, dst_out == NULL, TL_ALLOC_FAILURE, return excv);
    (*dst_out)->data = malloc(sizeof(uint8_t) * src->flength * src->fwidth);
    CHECK(excv, (*dst_out)->data == NULL, TL_ALLOC_FAILURE, return excv);

    memcpy((*dst_out)->data, src->data, sizeof(uint8_t) * src->flength * src->fwidth);
    (*dst_out)->flength = src->flength;
    (*dst_out)->fwidth = src->fwidth;
    return excv;
}

void destroy_media_mtdta(media_mtdta **mtdta_ptr) {
    if (mtdta_ptr == NULL || *mtdta_ptr == NULL) {
        return;
    }
    free((*mtdta_ptr)->media_path);
    free(*mtdta_ptr);
    *mtdta_ptr = NULL;
}

void destroy_thread_data(thread_data **thdta_ptr) {
    if (thdta_ptr == NULL || *thdta_ptr == NULL) {
        return;
    }
    free(*thdta_ptr);
    *thdta_ptr = NULL;
}

void destroy_player(player **pl_ptr) {
    if (pl_ptr == NULL || *pl_ptr == NULL) {
        return;
    }
    if ((*pl_ptr)->th_hndles) {
        set_atomic_bool(&(*pl_ptr)->shutdown, true);
        SetEvent((*pl_ptr)->ev_hndles[AUDIO_PROD_EVENT_WAKE_HNDLE]);
        if ((*pl_ptr)->media_mtdta->video_present) {
            SetEvent((*pl_ptr)->ev_hndles[VIDEO_PROD_EVENT_WAKE_HNDLE]);
        }
        WaitForMultipleObjects((*pl_ptr)->active_threads, (*pl_ptr)->th_hndles, true, INFINITE);
        for (size_t i = 0; i < (*pl_ptr)->active_threads; ++i) {
            if ((*pl_ptr)->th_hndles == NULL) {
                continue;
            }
            CloseHandle((*pl_ptr)->th_hndles[i]);
        }
        free((*pl_ptr)->th_hndles);
    }
    if ((*pl_ptr)->th_data) {
        for (size_t i = 0; i < (*pl_ptr)->active_threads; ++i) {
            destroy_thread_data(&((*pl_ptr)->th_data[i]));
        }
        free((*pl_ptr)->th_data);
    }
    if ((*pl_ptr)->ev_hndles) {
        for (size_t i = 0; i < MAX_EVENTS; ++i) {
            if ((*pl_ptr)->ev_hndles == NULL) {
                continue;
            }
            CloseHandle((*pl_ptr)->ev_hndles[i]);
        }
        free((*pl_ptr)->ev_hndles);
    }
    free((*pl_ptr)->gwcvbuffer);
    free((*pl_ptr)->gwpvbuffer);
    if ((*pl_ptr)->video_rbuffer != NULL) {
        for (size_t i = 0; i < VBUFFER_BSIZE / sizeof(con_frame *); ++i) {
            destroy_conframe(&(*pl_ptr)->video_rbuffer[i]);
        }
    }

    free((*pl_ptr)->video_rbuffer);
    free((*pl_ptr)->audio_rbuffer);
    destroy_media_mtdta(&(*pl_ptr)->media_mtdta);
    free(*pl_ptr);
    *pl_ptr = NULL;
}

void destroy_conframe(con_frame **frame_ptr) {
    if (frame_ptr == NULL || *frame_ptr == NULL) {
        return;
    }
    free((*frame_ptr)->compressed_data);
    free(*frame_ptr);
    *frame_ptr = NULL;
}

void destroy_rawframe(raw_frame **frame_ptr) {
    if (frame_ptr == NULL || *frame_ptr == NULL) {
        return;
    }
    free((*frame_ptr)->data);
    free(*frame_ptr);
    *frame_ptr = NULL;
}

void state_print(player *pl) {
    AcquireSRWLockShared(&pl->srw_mclock);
    const double main_clock = get_atomic_double(&pl->main_clock);
    ReleaseSRWLockShared(&pl->srw_mclock);
    COORD c = {.X = 0, .Y = 0};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
    fprintf(
        stdout,
        "SHUTDOWN: %s \n"
        "PLAYING: %s \n"
        "LOOPING: %s \n"
        "INVALIDATED: %s \n"
        "MUTED: %s \n"
        "MAIN_CLOCK: %lf \n"
        "VOLUME: %lf \n"
        "SEEK_SPEED: %lf \n"
        "SERIAL: %zu \n"
        "AREAD_IDX: %zu \n"
        "AWRITE_IDX: %zu \n"
        "VREAD_IDX: %zu \n"
        "VWRITE_IDX: %zu \n"
        "ACTIVE_THREADS: %u \n",
        get_atomic_bool(&pl->shutdown) ? " TRUE" : "FALSE",
        get_atomic_bool(&pl->playing) ? " TRUE" : "FALSE",
        get_atomic_bool(&pl->looping) ? " TRUE" : "FALSE",
        get_atomic_bool(&pl->invalidated) ? " TRUE" : "FALSE",
        get_atomic_bool(&pl->muted) ? " TRUE" : "FALSE", main_clock, get_atomic_double(&pl->volume),
        get_atomic_double(&pl->seek_speed), get_atomic_size_t(&pl->serial),
        get_atomic_size_t(&pl->aread_idx), get_atomic_size_t(&pl->awrite_idx),
        get_atomic_size_t(&pl->vread_idx), get_atomic_size_t(&pl->vwrite_idx),
        (uint32_t)pl->active_threads
    );
}