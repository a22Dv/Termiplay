#include "tl_errors.h"
#include "tl_pch.h"
#include "tl_types.h"
#include "tl_utils.h"
#include "tl_video.h"

static tl_result get_new_ffmpeg_instance(
    const double clock_start,
    const WCHAR *media_path,
    FILE       **ffmpeg_instance_out
);
static tl_result get_raw_frame(
    FILE       *ffmpeg_stream,
    raw_frame **f_out
);

static tl_result get_con_frame(
    const raw_frame *raw,
    const raw_frame *keyframe,
    con_frame      **c_out
);

tl_result vpthread_exec(thread_data *data) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, data == NULL, TL_NULL_ARG, return excv);
    player             *pl = data->player;
    FILE               *ffmpeg_stream = NULL;
    size_t              set_serial = 0;
    double              prod_vclock = 0.0;
    static const size_t fbuffer_count = VBUFFER_BSIZE / sizeof(con_frame *);

    raw_frame *keyframe = malloc(sizeof(raw_frame));
    raw_frame *staging_frame = malloc(sizeof(raw_frame));

    CHECK(excv, keyframe == NULL, TL_ALLOC_FAILURE, return excv);
    CHECK(excv, staging_frame == NULL, TL_ALLOC_FAILURE, return excv);

    keyframe->data = malloc(MAXIMUM_BUFFER_SIZE);
    staging_frame->data = malloc(MAXIMUM_BUFFER_SIZE);
    CHECK(excv, keyframe->data == NULL, TL_ALLOC_FAILURE, return excv);
    CHECK(excv, staging_frame->data == NULL, TL_ALLOC_FAILURE, return excv);

    keyframe->flength = 0;
    keyframe->fwidth = 0;
    staging_frame->flength = 0;
    staging_frame->fwidth = 0;

    while (true) {
        if (get_atomic_bool(&pl->shutdown)) {
            break;
        }
        if (get_atomic_size_t(&pl->serial) != set_serial) {
            if (ffmpeg_stream) {
                _pclose(ffmpeg_stream);
                ffmpeg_stream = NULL;
            }
            set_atomic_size_t(&pl->vread_idx, 0);
            set_atomic_size_t(&pl->vwrite_idx, 0);
            for (size_t i = 0; i < fbuffer_count; ++i) {
                destroy_conframe(&pl->video_rbuffer[i]);
            }
            set_serial = get_atomic_size_t(&pl->serial);
            while (get_atomic_bool(&pl->invalidated)) {
                Sleep(5);
            }
            prod_vclock = get_atomic_double(&pl->main_clock);
        }
        TRY(excv,
            get_new_ffmpeg_instance(
                prod_vclock, data->player->media_mtdta->media_path, &ffmpeg_stream
            ),
            goto epilogue);

        while (true) {
            if (feof(ffmpeg_stream) || get_atomic_bool(&pl->shutdown) ||
                get_atomic_size_t(&pl->serial) != set_serial) {
                break;
            }
            const size_t nwrite_idx = (get_atomic_size_t(&pl->vwrite_idx) + 1) % fbuffer_count;
            while (nwrite_idx == get_atomic_size_t(&pl->vread_idx)) {
                if (get_atomic_bool(&pl->shutdown) ||
                    get_atomic_size_t(&pl->serial) != set_serial) {
                    break;
                }
                Sleep(5);
            }
            if (get_atomic_size_t(&pl->serial) != set_serial || get_atomic_bool(&pl->shutdown)) {
                break;
            }
            TRY(excv, get_raw_frame(ffmpeg_stream, &staging_frame), goto epilogue);
            TRY(excv,
                get_con_frame(
                    staging_frame, keyframe, &pl->video_rbuffer[get_atomic_size_t(&pl->vwrite_idx)]
                ),
                goto epilogue);
            set_atomic_size_t(&pl->vwrite_idx, nwrite_idx);
            memcpy(
                keyframe->data, staging_frame->data,
                staging_frame->flength * staging_frame->fwidth * sizeof(uint8_t)
            );
            keyframe->flength = staging_frame->flength;
            keyframe->fwidth = staging_frame->fwidth;
        }
        if (feof(ffmpeg_stream)) {
            while (!get_atomic_bool(&pl->shutdown) &&
                   get_atomic_size_t(&pl->serial) == set_serial) {
                Sleep(1);
            }
        }
        _pclose(ffmpeg_stream);
        keyframe->flength = 0;
        keyframe->fwidth = 0;
        ffmpeg_stream = NULL;
    }
epilogue:
    // destroy_player() takes care of final free-ing after all threads have been shut down to
    // prevent use-after-free.
    if (ffmpeg_stream) {
        _pclose(ffmpeg_stream);
        ffmpeg_stream = NULL;
    }
    destroy_rawframe(&staging_frame);
    destroy_rawframe(&keyframe);
    return excv;
}

static tl_result get_new_ffmpeg_instance(
    const double clock_start,
    const WCHAR *media_path,
    FILE       **ffmpeg_instance_out
) {
    return TL_SUCCESS;
}

static tl_result get_raw_frame(
    FILE       *ffmpeg_stream,
    raw_frame **f_out
) {
    // f_out must be allocated for it, is guaranteed to not write more than MAX_BUFFER_SIZE.
    return TL_SUCCESS;
}

static tl_result get_con_frame(
    const raw_frame *raw,
    const raw_frame *keyframe,
    con_frame      **c_out
) {
    // A keyframe with 0 width & height means the full frame will be processed without
    // delta-diffing.
    return TL_SUCCESS;
}

tl_result vcthread_exec(thread_data *data) { return TL_SUCCESS; }