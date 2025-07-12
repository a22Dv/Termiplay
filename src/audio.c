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

static void audio_callback(
    ma_device  *pDevice,
    void       *pOutput,
    const void *pInput,
    ma_uint32   frameCount
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
    FILE               *ffmpeg_stream = NULL;
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
            set_atomic_size_t(&pl->aread_idx, 0);
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

        while (true) {
            if (feof(ffmpeg_stream)) {
                break;
            }
            if (get_atomic_size_t(&pl->serial) != set_serial || get_atomic_bool(&pl->shutdown)) {
                break;
            }
            size_t f_ret = fread(staging_buffer, sizeof(s16_le), staging_scount, ffmpeg_stream);

            // Only reached at the end of the file, or an error.
            if (f_ret != staging_scount) {
                memset(staging_buffer + f_ret, 0, (staging_scount - f_ret) * sizeof(s16_le));
            }
            for (size_t j = 0; j < staging_scount; ++j) {
                const size_t nwrite_idx = (get_atomic_size_t(&pl->awrite_idx) + 1) % abuffer_scount;
                while (nwrite_idx == get_atomic_size_t(&pl->aread_idx)) {
                    if (get_atomic_size_t(&pl->serial) != set_serial ||
                        get_atomic_bool(&pl->shutdown)) {
                        break;
                    }
                    Sleep(10);
                }
                if (get_atomic_size_t(&pl->serial) != set_serial) {
                    break;
                }
                pl->audio_rbuffer[get_atomic_size_t(&pl->awrite_idx)] = staging_buffer[j];
                set_atomic_size_t(&pl->awrite_idx, nwrite_idx);
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
        L"ffmpeg -v quiet -ss %lf -i \"%ls\" -f s16le -ar %i -ac %i "
        L"-vn -",
        clock_start, media_path, A_SAMP_RATE, A_CHANNELS
    );
    CHECK(excv, swret < 0, TL_FORMAT_FAILURE, return excv);
    *ffmpeg_instance_out = _wpopen(cmd, L"rb");
    CHECK(excv, *ffmpeg_instance_out == NULL, TL_PIPE_CREATION_FAILURE, return excv);
    return excv;
}

tl_result acthread_exec(thread_data *data) {
    tl_result excv = TL_SUCCESS;
    CHECK(excv, data == NULL, TL_NULL_ARG, return excv);
    player *pl = data->player;

    ma_device_config aconfig = ma_device_config_init(ma_device_type_playback);
    aconfig.playback.channels = A_CHANNELS;
    aconfig.sampleRate = A_SAMP_RATE;
    aconfig.pUserData = (void *)data;
    aconfig.playback.format = ma_format_s16;
    aconfig.dataCallback = audio_callback;
    ma_device adevice;
    ma_result initr = ma_device_init(NULL, &aconfig, &adevice);
    CHECK(excv, initr != MA_SUCCESS, TL_MINIAUDIO_ERR, return excv);
    ma_result devr = ma_device_start(&adevice);
    CHECK(excv, devr != MA_SUCCESS, TL_MINIAUDIO_ERR, goto epilogue);
    while (true) {
        if (get_atomic_bool(&pl->shutdown)) {
            break;
        }
        Sleep(100);
    }
epilogue:
    ma_device_uninit(&adevice);
    return excv;
}

static void audio_callback(
    ma_device  *pDevice,
    void       *pOutput,
    const void *pInput,
    ma_uint32   frameCount
) {
    thread_data        *data = (thread_data *)pDevice->pUserData;
    player             *pl = data->player;
    size_t              samples_required = frameCount * A_CHANNELS;
    static size_t       set_serial = 0;
    static const size_t buffer_samples = ABUFFER_BSIZE / sizeof(s16_le);

    set_serial = get_atomic_size_t(&pl->serial);
    const bool shutdown = get_atomic_bool(&pl->shutdown);
    const bool muted = get_atomic_bool(&pl->muted);
    const bool invalidated = get_atomic_bool(&pl->invalidated);
    const bool playback = get_atomic_bool(&pl->playing);
    const size_t current_serial = get_atomic_size_t(&pl->serial);
    const double volume = get_atomic_double(&pl->volume);

    if (shutdown || muted || invalidated || !playback || set_serial != current_serial) {
        memset(pOutput, 0, samples_required * sizeof(s16_le));
        if (set_serial != current_serial) {
            set_serial = current_serial;
            return;
        }
        if (!playback || invalidated || shutdown) {
            return;
        }
    }

    // Only this callback can increase or modify aread_idx.
    const size_t set_read = get_atomic_size_t(&pl->aread_idx);
    const size_t new_read = (set_read + samples_required) % (buffer_samples);
    const size_t set_write = get_atomic_size_t(&pl->awrite_idx);
    const size_t valid_samples = (set_write + (buffer_samples)-set_read) % (buffer_samples);

    if (valid_samples < samples_required) {
        memset(pOutput, 0, samples_required * sizeof(s16_le));
        return;
    }

    if (!muted && playback && !invalidated && !shutdown && valid_samples >= samples_required) {
        if (set_read + samples_required < (buffer_samples)) {
            memcpy(pOutput, &pl->audio_rbuffer[set_read], samples_required * sizeof(s16_le));
        } else {
            const size_t diff = samples_required - (buffer_samples - set_read);
            memcpy(
                pOutput, &pl->audio_rbuffer[set_read], (buffer_samples - set_read) * sizeof(s16_le)
            );
            memcpy(
                (s16_le *)pOutput + (buffer_samples - set_read), &pl->audio_rbuffer,
                diff * sizeof(s16_le)
            );
        }
        for (size_t i = 0; i < samples_required; ++i) {
            ((s16_le *)pOutput)[i] = (s16_le)((double)((s16_le *)pOutput)[i] * volume);
        }
    }
    if (valid_samples >= samples_required) {
        set_atomic_size_t(&pl->aread_idx, new_read);
    }

    AcquireSRWLockExclusive(&pl->srw_mclock);
    add_atomic_double(&pl->main_clock, (double)frameCount / (double)A_SAMP_RATE);
    ReleaseSRWLockExclusive(&pl->srw_mclock);
}
