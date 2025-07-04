#ifndef TL_TYPES
#define TL_TYPES
#include <Windows.h>
#include <stdint.h>
#include <stdbool.h>

#define AUDIO_SAMPLE_RATE 48000
#define VIDEO_FPS 30
#define CHANNEL_COUNT 2

#define AUDIO_THREAD_ID 0
#define PROC_THREAD_ID 1
#define VIDEO_THREAD_ID 2
#define WORKER_THREAD_COUNT 3

#define VIDEO_FLAG_OFFSET 4
#define AUDIO_FLAG_OFFSET 0
#define VIDEO_PRESENT (1)
#define AUDIO_PRESENT (1 << 4)

#define LOOP 'l'
#define MUTE 'm'
#define TOGGLE_CHARSET 'c'
#define TOGGLE_PLAYBACK ' '
#define QUIT 'q'
#define EXTENDED 0xE0
#define ARROW_UP 72
#define ARROW_DOWN 80
#define ARROW_LEFT 75
#define ARROW_RIGHT 77

#define POLLING_RATE_MS 50
#define SEEK_SPEEDS 8
#define MAX_BUFFER_COUNT 4
#define MAX_INDIV_FRAMEBUF_UNCOMP_SIZE 10'000'000

/// @brief Basic video metadata.
typedef struct metadata {
    uint16_t width;
    uint16_t height;
    uint8_t  streams_mask;
    uint8_t  fps;
    double   duration;
    WCHAR   *media_path;
} metadata;

/// BUG: REMOVE UNDERFLOW FLAGS.

/// @brief Active player state.
typedef struct player_state {
    bool    shutdown;
    bool    playback;
    bool    looping;
    bool    seeking;
    bool    muted;
    bool    default_charset;
    bool    vbuffer1_readable;
    bool    abuffer1_readable;
    bool    abuffer2_readable;
    bool    vbuffer2_readable;
    double  volume;
    double  main_clock;
    double  seek_variable;
    size_t  seek_idx;
    size_t  current_serial;
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

/// @brief Worker thread data.
typedef struct wthread_data {
    thread_data *thdata;
    HANDLE       order_event;
    HANDLE       finish_event;
    HANDLE       shutdown_event;
    uint8_t      wthread_id;
    char     *uncomp_fbuffer;
} wthread_data;

#endif