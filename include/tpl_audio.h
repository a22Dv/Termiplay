#ifndef TERMIPLAY_AUDIO
#define TERMIPLAY_AUDIO
#define DEBUG

#define MINIAUDIO_IMPLEMENTATION

#define SAMPLE_RATE 48000
#define BUFFER_SECONDS_LEN 3
#define CHANNELS 2

#define MAX_BUFFER_SIZE SAMPLE_RATE* BUFFER_SECONDS_LEN* CHANNELS

#include "miniaudio.h"
#include "tpl_errors.h"
#include "tpl_player.h"

static void tpl_audio_callback(
    ma_device*  pDevice,
    void*       pOutput,
    const void* pInput,
    ma_uint32   frameCount
) {
    tpl_audio_callback_data* ac_data    = (tpl_audio_callback_data*)pDevice->pUserData;
    const size_t             active_idx = _InterlockedOr(ac_data->active_idx, 0);

    AcquireSRWLockShared(&ac_data->thread_data->p_state->srw_lock);
    const bool   mute   = ac_data->thread_data->p_state->muted;
    const double volume = ac_data->thread_data->p_state->vol_lvl;
    ReleaseSRWLockShared(&ac_data->thread_data->p_state->srw_lock);

    // Fit within active buffer.
    if (MAX_BUFFER_SIZE >= active_idx + frameCount * CHANNELS) {
        memcpy(pOutput, &ac_data->buffer1[active_idx], frameCount * CHANNELS * sizeof(int16_t));
        int16_t* out_buffer = (int16_t*)pOutput;

        // Set flags, increment idx.
        _InterlockedExchange(ac_data->active_idx, active_idx + frameCount * CHANNELS);
        AcquireSRWLockExclusive(&ac_data->thread_data->p_state->srw_lock);
        ac_data->thread_data->p_state->main_clock += (double)frameCount / (double)SAMPLE_RATE;
        ReleaseSRWLockExclusive(&ac_data->thread_data->p_state->srw_lock);
    } else {
        // Does not fit within active buffer.
        const size_t remaining_samples = MAX_BUFFER_SIZE - active_idx;
        if (remaining_samples > 0) {
            memcpy(pOutput, &ac_data->buffer1[active_idx], remaining_samples * sizeof(int16_t));
        }

        // Possible page-fault. 576KB every transfer.
        memcpy(ac_data->buffer1, ac_data->buffer2, MAX_BUFFER_SIZE * sizeof(int16_t));
        _InterlockedExchange(ac_data->fill_flag, true);
        memcpy(
            (char*)pOutput + remaining_samples * sizeof(int16_t), ac_data->buffer1,
            (frameCount * CHANNELS - remaining_samples) * sizeof(int16_t)
        );

        // Set flags, increment idx.
        _InterlockedExchange(ac_data->active_idx, (frameCount * CHANNELS - remaining_samples));
        AcquireSRWLockExclusive(&ac_data->thread_data->p_state->srw_lock);
        ac_data->thread_data->p_state->main_clock += (double)frameCount / (double)SAMPLE_RATE;
        ReleaseSRWLockExclusive(&ac_data->thread_data->p_state->srw_lock);
    }

    // Apply volume.
    int16_t* out_buffer = (int16_t*)pOutput;
    for (size_t i = 0; i < frameCount * CHANNELS; ++i) {
        float value = out_buffer[i];
        value *= mute ? 0.0 : 1.0;
        value *= (float)volume;
        out_buffer[i] = (int16_t)value;
    }
}

/// @brief
/// @param thread_data
/// @return
static tpl_result tpl_execute_audio_thread(tpl_thread_data* thread_data) {

    // Set variables.
    volatile LONG            fill_buffer_flag = 0;
    volatile LONG            active_idx       = 0;
    tpl_result               return_code      = TPL_SUCCESS;
    int16_t*                 audio_buffer1    = NULL;
    int16_t*                 audio_buffer2    = NULL;
    tpl_audio_callback_data* tpl_acd          = NULL;
    ma_device_config         audio_config     = ma_device_config_init(ma_device_type_playback);
    ma_device                audio_device;
    bool                     last_playback_state = true;
    bool                     was_seeking         = false;

    // Create two buffers
    audio_buffer1 = malloc(sizeof(int16_t) * MAX_BUFFER_SIZE);
    audio_buffer2 = malloc(sizeof(int16_t) * MAX_BUFFER_SIZE);
    IF_ERR_GOTO(audio_buffer1 == NULL, TPL_ALLOC_FAILED, return_code);
    IF_ERR_GOTO(audio_buffer2 == NULL, TPL_ALLOC_FAILED, return_code);

    // Fill buffers.
    tpl_result fill_result1 =
        tpl_fill_audio_buffer(0.0, audio_buffer1, thread_data->p_conf->video_fpath);
    tpl_result fill_result2 = tpl_fill_audio_buffer(
        (float)BUFFER_SECONDS_LEN, audio_buffer2, thread_data->p_conf->video_fpath
    );
    IF_ERR_GOTO(tpl_failed(fill_result1), fill_result1, return_code);
    IF_ERR_GOTO(tpl_failed(fill_result2), fill_result2, return_code);

    // Get callback data.
    tpl_result callback_data_result = get_callback_data(
        &tpl_acd, thread_data, audio_buffer1, audio_buffer2, &fill_buffer_flag, &active_idx
    );
    IF_ERR_GOTO(tpl_failed(callback_data_result), callback_data_result, return_code);

    // Device configuration.
    audio_config.playback.format   = ma_format_s16;
    audio_config.playback.channels = CHANNELS;
    audio_config.sampleRate        = SAMPLE_RATE;
    audio_config.dataCallback      = tpl_audio_callback;
    audio_config.pUserData         = tpl_acd;
    ma_result dev_init_call        = ma_device_init(NULL, &audio_config, &audio_device);
    IF_ERR_GOTO(dev_init_call != MA_SUCCESS, TPL_FAILED_TO_INITIALIZE_AUDIO, return_code);
    ma_result start_call = ma_device_start(&audio_device);
    IF_ERR_GOTO(start_call != MA_SUCCESS, TPL_FAILED_TO_START_AUDIO, return_code);

    // Audio loop.
    while (true) {

        // Shutdown.
        if (_InterlockedOr(thread_data->shutdown_ptr, 0)) {
            goto unwind;
        }

        // Fill, callback-requested.
        if (_InterlockedOr(&fill_buffer_flag, 0)) {
            AcquireSRWLockShared(&thread_data->p_state->srw_lock);
            AcquireSRWLockShared(&thread_data->p_conf->srw_lock);
            const double sec_start = thread_data->p_state->main_clock + BUFFER_SECONDS_LEN;
            const wpath* v_path    = thread_data->p_conf->video_fpath;
            ReleaseSRWLockShared(&thread_data->p_conf->srw_lock);
            ReleaseSRWLockShared(&thread_data->p_state->srw_lock);
            tpl_fill_audio_buffer(sec_start, audio_buffer2, v_path);
            _InterlockedExchange(&fill_buffer_flag, false);
        }

        // User-controlled.
        if (!thread_data->p_state->playing && last_playback_state != false) {
            ma_device_stop(&audio_device);
            last_playback_state = false;
        } else if (thread_data->p_state->seeking && last_playback_state != false) {
            ma_device_stop(&audio_device);
            memset(audio_buffer1, 0, MAX_BUFFER_SIZE * sizeof(int16_t));
            memset(audio_buffer2, 0, MAX_BUFFER_SIZE * sizeof(int16_t));
            _InterlockedExchange(tpl_acd->active_idx, 0);
            last_playback_state = false;
            was_seeking         = true;
        } else if (thread_data->p_state->playing && last_playback_state != true) {
            if (was_seeking) {
                AcquireSRWLockShared(&thread_data->p_state->srw_lock);
                AcquireSRWLockShared(&thread_data->p_conf->srw_lock);
                const double sec_start = thread_data->p_state->main_clock;
                const wpath* v_path    = thread_data->p_conf->video_fpath;
                ReleaseSRWLockShared(&thread_data->p_conf->srw_lock);
                ReleaseSRWLockShared(&thread_data->p_state->srw_lock);
                tpl_result tpf1 = tpl_fill_audio_buffer(sec_start, audio_buffer1, v_path);
                IF_ERR_GOTO(tpl_failed(tpf1), tpf1, return_code);
                tpl_result tpf2 =
                    tpl_fill_audio_buffer(sec_start + BUFFER_SECONDS_LEN, audio_buffer2, v_path);
                IF_ERR_GOTO(tpl_failed(tpf2), tpf2, return_code);
                was_seeking = false;
            }
            ma_device_start(&audio_device);
            last_playback_state = true;
        }
        Sleep(1);
    }

unwind:
    return return_code;
}

/// @brief Fills an audio buffer with a call.
/// @param sec_start Start at this time-stamp.d
/// @param audio_buffer Buffer to fill. Must be empty. Will overwrite data.
/// @return Return code.
static tpl_result tpl_fill_audio_buffer(
    const double sec_start,
    int16_t*     audio_buffer,
    const wpath* video_fpath
) {
    wchar_t command[1024];
    _snwprintf(
        command, 1024,
        L"ffmpeg -i \"%ls\" -ss %.2lf -f s16le -t %i -vn -acodec pcm_s16le -ar %i -ac %i -vn -v "
        L"error "
        L"-hide_banner -",
        wstr_c_const(video_fpath), sec_start, BUFFER_SECONDS_LEN, SAMPLE_RATE, CHANNELS
    );
    FILE*  exec_command  = _wpopen(command, L"rb");
    size_t bytes_to_read = MAX_BUFFER_SIZE * sizeof(int16_t);
    size_t bytes_read    = 0;
    while (bytes_read != bytes_to_read) {
        int b_read =
            fread((char*)audio_buffer + bytes_read, 1, bytes_to_read - bytes_read, exec_command);
        if (b_read == 0) {
            break;
        }
        bytes_read += b_read;
    }
    int exec_code = _pclose(exec_command);
    if (exec_code != 0) {
        LOG_ERR(TPL_FAILED_TO_PIPE);
        return TPL_FAILED_TO_PIPE;
    }
    if (bytes_read < bytes_to_read) {
        memset(&audio_buffer[bytes_read / sizeof(int16_t)], 0, (bytes_to_read - bytes_read));
    }
    return TPL_SUCCESS;
}

/// @brief
/// @param buffer
/// @param thread_data
/// @param audio_buffer1
/// @param audio_buffer2
/// @return
static tpl_result get_callback_data(
    tpl_audio_callback_data** buffer,
    tpl_thread_data*          thread_data,
    int16_t*                  audio_buffer1,
    int16_t*                  audio_buffer2,
    volatile LONG*            fill_flag,
    volatile LONG*            active_idx
) {
    tpl_audio_callback_data* tpl_acd = malloc(sizeof(tpl_audio_callback_data));

    tpl_acd->buffer1     = audio_buffer1;
    tpl_acd->buffer2     = audio_buffer2;
    tpl_acd->thread_data = thread_data;
    tpl_acd->fill_flag   = fill_flag;
    tpl_acd->active_idx  = active_idx;
    *buffer              = tpl_acd;
    return TPL_SUCCESS;
}

#endif