#define MINIAUDIO_IMPLEMENTATION
#include "tl_audio.h"

tl_result audio_thread_exec(thread_data *thdata) {
    tl_result        exit_code = TL_SUCCESS;
    ma_device        audio_device;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.sampleRate = AUDIO_SAMPLE_RATE;
    config.playback.format = ma_format_s16;
    config.playback.channels = CHANNEL_COUNT;
    config.dataCallback = callback;
    config.pUserData = (void *)thdata;
    CHECK(
        exit_code, ma_device_init(NULL, &config, &audio_device) != MA_SUCCESS, TL_MA_ERROR,
        return exit_code
    );
    ma_device_start(&audio_device);
    while (true) {
        AcquireSRWLockShared(&thdata->pstate->control_srw);
        const bool shutdown_sig = thdata->pstate->shutdown;
        ReleaseSRWLockShared(&thdata->pstate->control_srw);
        if (shutdown_sig) {
            break;
        }
        Sleep(10);
    }
    ma_device_uninit(&audio_device);
    return exit_code;
}

void callback(
    ma_device  *pDevice,
    void       *pOutput,
    const void *pInput,
    ma_uint32   frameCount
) {
    const static double inverse = (double)1.0 / (double)AUDIO_SAMPLE_RATE;
    static size_t       current_serial = 0;
    static size_t       current_idx = 0;
    static const size_t max_buffer_samples = AUDIO_SAMPLE_RATE * CHANNEL_COUNT;
    thread_data        *thdata = (thread_data *)pDevice->pUserData;
    int16_t            *buffer1 = thdata->audio_buffer1;
    int16_t            *buffer2 = thdata->audio_buffer2;
    bool                new_buffer_request = false;

    AcquireSRWLockShared(&thdata->pstate->buffer_serial_srw);
    const bool   readable1 = thdata->pstate->abuffer1_readable;
    const bool   readable2 = thdata->pstate->abuffer2_readable;
    const size_t data_serial = thdata->pstate->current_serial;
    ReleaseSRWLockShared(&thdata->pstate->buffer_serial_srw);
    AcquireSRWLockShared(&thdata->pstate->control_srw);
    const bool   shutdown_sig = thdata->pstate->shutdown;
    const bool   playback = thdata->pstate->playback;
    const bool   seeking = thdata->pstate->seeking;
    const bool   muted = thdata->pstate->muted;
    const double volume = thdata->pstate->volume;
    ReleaseSRWLockShared(&thdata->pstate->control_srw);

    const size_t samples_count = frameCount * CHANNEL_COUNT;
    if (data_serial != current_serial) {
        current_serial = data_serial;
        current_idx = 0;
    }
    if (!playback || seeking || !readable1 || shutdown_sig) {
        memset(pOutput, 0, sizeof(int16_t) * samples_count);
        return;
    }
    if (current_idx + samples_count < max_buffer_samples) {
        memcpy(pOutput, &buffer1[current_idx], sizeof(int16_t) * samples_count);
        current_idx += samples_count;
    } else if (readable2) {
        memcpy(
            pOutput, &buffer1[current_idx], sizeof(int16_t) * (max_buffer_samples - current_idx)
        );
        memcpy(buffer1, buffer2, sizeof(int16_t) * max_buffer_samples);
        memcpy(
            (int16_t *)pOutput + (max_buffer_samples - current_idx), buffer1,
            sizeof(int16_t) * (samples_count - (max_buffer_samples - current_idx))
        );
        new_buffer_request = true;
        current_idx = samples_count - (max_buffer_samples - current_idx);
    } else {
        system("cls; pause; echo FALSE TRIGGER");
        memset(pOutput, 0, sizeof(int16_t) * CHANNEL_COUNT * frameCount);
        return;
    }
    for (size_t i = 0; i < samples_count; ++i) {
        ((int16_t *)pOutput)[i] =
            (int16_t)((double)((int16_t *)pOutput)[i] * volume) * (muted ? 0 : 1);
    }
    AcquireSRWLockExclusive(&thdata->pstate->clock_srw);
    thdata->pstate->main_clock += (double)frameCount * inverse;
    ReleaseSRWLockExclusive(&thdata->pstate->clock_srw);
    if (new_buffer_request) {
        AcquireSRWLockExclusive(&thdata->pstate->buffer_serial_srw);
        thdata->pstate->abuffer2_readable = false;
        new_buffer_request = false;
        ReleaseSRWLockExclusive(&thdata->pstate->buffer_serial_srw);
    }    
}