#include "tl_audio.h"
#include "tl_errors.h"
#include "tl_pch.h"
#include "tl_types.h"
#include "tl_utils.h"

static tl_result get_new_ffmpeg_instance(
    const double clock_start,
    const WCHAR *media_path,
    FILE       **ffmpeg_instance_out
);

tl_result apthread_exec(thread_data *data) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, data == NULL, TL_NULL_ARG, return excv);
    player             *pl = data->player;
    size_t              set_serial = 0;
    static double       prod_aclock = 0.0;
    s16_le              staging_buffer[GLBUFFER_BSIZE];
    static const size_t staging_scount = sizeof(staging_buffer) / sizeof(s16_le);  
    static const size_t abuffer_scount = ABUFFER_BSIZE / sizeof(s16_le);
    FILE *ffmpeg_stream = NULL;
    while (true) {
        if (get_atomic_bool(&pl->shutdown)) {
            break;
        }
        if (get_atomic_size_t(&pl->serial) != set_serial) {
            if (ffmpeg_stream != NULL) {
                _pclose(ffmpeg_stream);
                ffmpeg_stream = NULL;
            }
            set_serial = get_atomic_size_t(&pl->serial);
            set_atomic_size_t(&pl->awrite_idx, 0);
            while (get_atomic_bool(&pl->invalidated)) {
                Sleep(5);
            }
            prod_aclock = get_atomic_double(&pl->main_clock);
        }
        TRY(excv,
            get_new_ffmpeg_instance(
                prod_aclock, data->player->media_mtdta->media_path, &ffmpeg_stream
            ),
            goto epilogue);
        prod_aclock += 1.0;
        for (size_t i = 0; i < abuffer_scount / staging_scount; ++i) {
            if (get_atomic_size_t(&pl->serial) != set_serial || get_atomic_bool(&pl->shutdown)) {
                break;
            }
            size_t f_ret = fread(staging_buffer, sizeof(s16_le), staging_scount, ffmpeg_stream);

            // Only reached at the end of the file, or an error.
            if (f_ret != staging_scount) {
                memset(staging_buffer + f_ret, 0, (staging_scount - f_ret) * sizeof(s16_le));
            }
            for (size_t j = 0; j < staging_scount; ++j) {
                while ((get_atomic_size_t(&pl->awrite_idx) + 1) % abuffer_scount ==
                       get_atomic_size_t(&pl->aread_idx)) {
                    if (get_atomic_size_t(&pl->serial) != set_serial || get_atomic_bool(&pl->shutdown)) {
                        break;
                    }
                    Sleep(5);
                }
                if (get_atomic_size_t(&pl->serial) != set_serial) {
                    break;
                }
                const size_t write_idx = (get_atomic_size_t(&pl->awrite_idx) + 1) % abuffer_scount;
                pl->audio_rbuffer[write_idx] = staging_buffer[j];
                set_atomic_size_t(&pl->awrite_idx, write_idx);
            }
        }
        _pclose(ffmpeg_stream);
        ffmpeg_stream = NULL;
    }
epilogue:
    if (ffmpeg_stream) {
        _pclose(ffmpeg_stream);
        ffmpeg_stream = NULL;
    }
    return excv;
}

static tl_result get_new_ffmpeg_instance(
    const double clock_start,
    const WCHAR *media_path,
    FILE       **ffmpeg_instance_out
) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, clock_start < 0.0, TL_INVALID_ARG, return excv);
    CHECK(excv, media_path == NULL, TL_NULL_ARG, return excv);
    CHECK(excv, ffmpeg_instance_out == NULL, TL_NULL_ARG, return excv);

    wchar_t cmd[GBUFFER_BSIZE];
    int     swret = swprintf_s(
        cmd, GBUFFER_BSIZE,
        L"ffmpeg -v quiet -ss %lf -i \"%ls\"-f s16le -t 1 -ar %i -ac %i "
        L"-vn -",
        clock_start, media_path, A_SAMP_RATE, A_CHANNELS
    );
    CHECK(excv, swret < 0, TL_FORMAT_FAILURE, return excv);
    *ffmpeg_instance_out = _wpopen(cmd, L"rb");
    CHECK(excv, *ffmpeg_instance_out == NULL, TL_PIPE_CREATION_FAILURE, return excv);
    return excv;
}

tl_result acthread_exec(thread_data *data) { 
    return TL_SUCCESS; 
}