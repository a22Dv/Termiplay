#ifndef TL_TYPES
#define TL_TYPES
#include <stdint.h>

#define AUDIO_SAMPLE_RATE 48000
#define VIDEO_FPS 30
#define CHANNEL_COUNT 2

#define AUDIO_THREAD_ID 0
#define VIDEO_THREAD_ID 1
#define PROC_THREAD_ID 2
#define WORKER_THREAD_COUNT 3

#define CHARSET_1 " `.-':_,^=;><+!rc*/z?sLTv)J7}fI31tlu[neoZ5Yxjya24VpOGbUAKXHm8RD#$Bg0MNWQ&@"
#define CHARSET_2 "▀"

/// @brief Basic video metadata.
typedef struct metadata {
    uint16_t width;
    uint16_t height;
    uint8_t  fps;
    double   duration;
} metadata;

/// @brief Active player state.
typedef struct player_state {
    bool    shutdown;
    bool    playback;
    bool    looping;
    bool    seeking;
    bool    muted;
    double  volume;
    double  main_clock;
    size_t  seek_idx;
    SRWLOCK srw;
} player_state;

/// @brief Thread data.
typedef struct thread_data {
    char        **video_buffer1;
    char        **video_buffer2;
    int16_t      *audio_buffer1;
    int16_t      *audio_buffer2;
    player_state *pstate;
    metadata     *mtdta;
    uint8_t       thread_id;

} thread_data;

#endif