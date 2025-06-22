#ifndef TERMIPLAY_AUDIO
#define TERMIPLAY_AUDIO
#define DEBUG

#define MINIAUDIO_IMPLEMENTATION

#define SAMPLE_RATE 48000
#define BUFFER_SECONDS_LEN 3
#define CHANNELS 2

#define PI 3.1415926535
#define FREQUENCY_DEBUG 300

#include "miniaudio.h"
#include "tpl_errors.h"
#include "tpl_player.h"

static void tpl_audio_callback(
    ma_device*  pDevice,
    void*       pOutput,
    const void* pInput,
    ma_uint32   frameCount
) {
    double volume = ((tpl_player_state*)(pDevice->pUserData))->vol_lvl;
    wprintf(L"%lf\r", volume);
    static double phase           = 0;
    int16_t*      out             = (int16_t*)pOutput;
    ma_uint32     channels        = pDevice->playback.channels;
    double        sample_rate     = pDevice->sampleRate;
    double        phase_increment = 2.0 * PI * FREQUENCY_DEBUG / sample_rate;
    for (ma_uint32 i = 0; i < frameCount; ++i) {
        double sample = sin(phase);
        sample *= volume;
        int16_t s16    = (int16_t)(sample * 30000);
        for (ma_uint32 ch = 0; ch < channels; ++ch) {
            *out++ = s16;
        }
        phase += phase_increment;
        if (phase >= 2.0 * PI) {
            phase -= 2.0 * PI;
        }
    }
}

static tpl_result tpl_execute_audio_thread(tpl_thread_data* thread_data) {
    // Create two buffers
    int16_t* audio_buffer1 = malloc(sizeof(int16_t) * SAMPLE_RATE * BUFFER_SECONDS_LEN * CHANNELS);
    int16_t* audio_buffer2 = malloc(sizeof(int16_t) * SAMPLE_RATE * BUFFER_SECONDS_LEN * CHANNELS);

    // Device configuration.
    ma_device_config audio_config  = ma_device_config_init(ma_device_type_playback);
    audio_config.playback.format   = ma_format_s16;
    audio_config.playback.channels = CHANNELS;
    audio_config.sampleRate        = SAMPLE_RATE;
    audio_config.dataCallback      = tpl_audio_callback;
    audio_config.pUserData         = thread_data->p_state;

    ma_device audio_device;
    ma_result dev_init_call = ma_device_init(NULL, &audio_config, &audio_device);
    if (dev_init_call != MA_SUCCESS) {
        LOG_ERR(TPL_FAILED_TO_INITIALIZE_AUDIO);
        return TPL_FAILED_TO_INITIALIZE_AUDIO;
    }
    ma_result start_call = ma_device_start(&audio_device);
    if (start_call != MA_SUCCESS) {
        LOG_ERR(TPL_FAILED_TO_START_AUDIO);
        return TPL_FAILED_TO_START_AUDIO;
    }

    bool last_playback_state = true;
    while (true) {
        if (InterlockedOr(thread_data->shutdown_ptr, 0)) {
            return TPL_SUCCESS;
        }
        if (!thread_data->p_state->playing && last_playback_state != false) {
            ma_device_stop(&audio_device);
            last_playback_state = false;
        } else if (thread_data->p_state->playing && last_playback_state != true) {
            ma_device_start(&audio_device);
            last_playback_state = true;
        }
        Sleep(1);
    }
}

static void swap_active_buffer(
    int16_t*  inactive_buffer,
    int16_t** current_buffer_pointer
) {
    *current_buffer_pointer = inactive_buffer;
}

#endif