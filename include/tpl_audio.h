#ifndef TERMIPLAY_AUDIO
#define TERMIPLAY_AUDIO
#define DEBUG

#define MINIAUDIO_IMPLEMENTATION

#define SAMPLE_RATE 48000
#define BUFFER_SECONDS_LEN 3
#define CHANNELS 2

#include "miniaudio.h"
#include "tpl_errors.h"
#include "tpl_player.h"

static void tpl_audio_callback(
    ma_device*  pDevice,
    void*       pOutput,
    const void* pInput,
    ma_uint32   frameCount
) {

    /// BUG: current active index is flawed. Seek behavior will be buggy. Must fix.
    int16_t*                 pOutputCast        = (int16_t*)pOutput;
    static size_t            current_active_idx = 0;
    tpl_audio_callback_data* athread_data       = (tpl_audio_callback_data*)pDevice->pUserData;
    if (current_active_idx + frameCount * CHANNELS <= SAMPLE_RATE * BUFFER_SECONDS_LEN * CHANNELS) {
        memcpy(
            pOutput, &athread_data->buffer1[current_active_idx],
            frameCount * sizeof(int16_t) * CHANNELS
        );
        current_active_idx += frameCount * CHANNELS;
        athread_data->thread_data->p_state->main_clock += (double)frameCount / (double)SAMPLE_RATE;
        return;
    }
    memcpy(
        pOutput, &athread_data->buffer1[current_active_idx],
        (SAMPLE_RATE * BUFFER_SECONDS_LEN * CHANNELS - current_active_idx * CHANNELS) *
            sizeof(int16_t)
    );
    memcpy(
        &athread_data->buffer1, &athread_data->buffer2,
        SAMPLE_RATE * BUFFER_SECONDS_LEN * CHANNELS * sizeof(int16_t)
    );
    current_active_idx = frameCount - current_active_idx / CHANNELS;
    _InterlockedExchange(athread_data->fill_flag, true);
}

/// @brief
/// @param thread_data
/// @return
static tpl_result tpl_execute_audio_thread(tpl_thread_data* thread_data) {

    // Set variables.
    volatile LONG            fill_buffer_flag = 0;
    tpl_result               return_code      = TPL_SUCCESS;
    int16_t*                 audio_buffer1    = NULL;
    int16_t*                 audio_buffer2    = NULL;
    tpl_audio_callback_data* tpl_acd          = NULL;
    ma_device_config         audio_config     = ma_device_config_init(ma_device_type_playback);
    ma_device                audio_device;
    bool                     last_playback_state = true;
    bool                     was_seeking         = false;

    // Create two buffers
    audio_buffer1 = malloc(sizeof(int16_t) * SAMPLE_RATE * BUFFER_SECONDS_LEN * CHANNELS);
    audio_buffer2 = malloc(sizeof(int16_t) * SAMPLE_RATE * BUFFER_SECONDS_LEN * CHANNELS);
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
    tpl_result callback_data_result =
        get_callback_data(&tpl_acd, thread_data, audio_buffer1, audio_buffer2, &fill_buffer_flag);
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
        if (_InterlockedOr(thread_data->shutdown_ptr, 0)) {
            goto unwind;
        }
        AcquireSRWLockShared(&thread_data->p_state->srw_lock);
        if (!thread_data->p_state->playing && last_playback_state != false) {
            ma_device_stop(&audio_device);
            last_playback_state = false;
        } else if (thread_data->p_state->seeking && last_playback_state != false) {
            ma_device_stop(&audio_device);
            memset(audio_buffer1, 0, SAMPLE_RATE * CHANNELS * BUFFER_SECONDS_LEN * sizeof(int16_t));
            memset(audio_buffer2, 0, SAMPLE_RATE * CHANNELS * BUFFER_SECONDS_LEN * sizeof(int16_t));
            last_playback_state = false;
            was_seeking         = true;
        } else if (thread_data->p_state->playing && last_playback_state != true) {
            if (was_seeking) {
                AcquireSRWLockShared(&thread_data->p_conf->srw_lock);
                tpl_result tpf1 = tpl_fill_audio_buffer(
                    thread_data->p_state->main_clock, audio_buffer1,
                    thread_data->p_conf->video_fpath
                );
                ReleaseSRWLockShared(&thread_data->p_conf->srw_lock);
                IF_ERR_GOTO(tpl_failed(tpf1), tpf1, return_code);

                AcquireSRWLockShared(&thread_data->p_conf->srw_lock);
                tpl_result tpf2 = tpl_fill_audio_buffer(
                    thread_data->p_state->main_clock + 3.0, audio_buffer2,
                    thread_data->p_conf->video_fpath
                );
                ReleaseSRWLockShared(&thread_data->p_conf->srw_lock);
                IF_ERR_GOTO(tpl_failed(tpf2), tpf2, return_code);
                was_seeking = false;
            }
            ma_device_start(&audio_device);
            last_playback_state = true;
        }
        ReleaseSRWLockShared(&thread_data->p_state->srw_lock);
        if (_InterlockedOr(&fill_buffer_flag, 0)) {
            tpl_fill_audio_buffer(
                thread_data->p_state->main_clock, audio_buffer2, thread_data->p_conf->video_fpath
            );
            _InterlockedExchange(&fill_buffer_flag, false);
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
    size_t bytes_to_read = BUFFER_SECONDS_LEN * SAMPLE_RATE * CHANNELS * sizeof(int16_t);
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
    volatile LONG*            fill_flag
) {
    tpl_audio_callback_data* tpl_acd = malloc(sizeof(tpl_audio_callback_data));

    tpl_acd->buffer1     = audio_buffer1;
    tpl_acd->buffer2     = audio_buffer2;
    tpl_acd->thread_data = thread_data;
    tpl_acd->fill_flag   = fill_flag;
    *buffer              = tpl_acd;
    return TPL_SUCCESS;
}

#endif