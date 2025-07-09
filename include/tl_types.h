#pragma once

#include <Windows.h>
#include <stdint.h>
#include "tl_errors.h"

#define V_FPS 30
#define A_SAMP_RATE 48000
#define A_CHANNELS 2
#define V_FRAME_INTERVALS 0.0334
#define GBUFFER_BSIZE 512        // Generic buffer size.
#define GLBUFFER_BSIZE 1024      // Generic long buffer size.
#define GWVBUFFER_BSIZE 10000000 // Generic work video buffer size. 10MB.
#define VBUFFER_BSIZE 2 * V_FPS * sizeof(char *)
#define ABUFFER_BSIZE 2 * A_SAMP_RATE *A_CHANNELS * sizeof(int16_t)

typedef volatile LONG32 atomic_bool;
typedef volatile LONG64 atomic_size_t;
typedef volatile LONG64 atomic_double;

/// @brief Handle index. 
typedef enum handle_idx {
    AUDIO_THREAD_HNDLE,
    VIDEO_THREAD_HNDLE,
    AUDIO_PROD_THREAD_HNDLE,
    VIDEO_PROD_THREAD_HNDLE,
    AUDIO_PROD_WAKE_HNDLE,
    VIDEO_PROD_WAKE_HNDLE,
} handle_idx;

/// @brief Player.
typedef struct player {
    char **const   video_rbuffer;
    int16_t *const audio_rbuffer;
    char *const    gvwbuffer; // Generic work buffer allocation specifically for video processing.
    atomic_bool    shutdown;
    atomic_bool    playing;
    atomic_bool    looping;
    atomic_bool    invalidated; // Any disruptive operation. (Seeking, console resizing)
    atomic_double  main_clock;
    atomic_double  volume;
    atomic_double  seek_speed;
    atomic_size_t  serial; // Data versioning.
    atomic_size_t  vread_idx;
    atomic_size_t  aread_idx;
    const HANDLE const *hndles; // Use with `handle_idx`.
} player;

/// @brief Media metadata.
typedef struct media_mtdta {
    const WCHAR *const media_path;
    const double       duration;
    const size_t       height;
    const size_t       width;
} media_mtdta;

/// @brief Thread IDs
typedef enum thread_id {
    MAIN_THREAD_ID,
    AUDIO_THREAD_ID,
    VIDEO_THREAD_ID,
    AUDIO_PROD_THREAD_ID,
    VIDEO_PROD_THREAD_ID
} thread_id;

/// @brief Thread data to be passed at creation.
typedef struct thread_data {
    player *const            player;
    const media_mtdta *const media_mtdta;
    const thread_id          thread_id;
} thread_data;
