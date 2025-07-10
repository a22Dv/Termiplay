#pragma once

#include <Windows.h>
#include <stdint.h>
#include "tl_errors.h"

#define MAX_THREADS 4
#define MAX_EVENTS 2
#define V_FPS 30
#define A_SAMP_RATE 48000
#define A_CHANNELS 2
#define V_FRAME_INTERVALS 0.0334
#define GBUFFER_BSIZE 512        // Generic buffer size.
#define GLBUFFER_BSIZE 1024      // Generic long buffer size.
#define GWVBUFFER_BSIZE 20971520 // Generic work video buffer size. 20MiB
#define VBUFFER_BSIZE 2 * V_FPS * sizeof(char *)
#define ABUFFER_BSIZE 2 * A_SAMP_RATE *A_CHANNELS * sizeof(int16_t)

typedef volatile LONG32 atomic_bool_t;
typedef volatile LONG64 atomic_size_t;
typedef volatile LONG64 atomic_double_t;
typedef int16_t         s16_le;

/// @brief Handle index.
/// @note Order is crucial to WaitForMultipleObjects(). Do not touch.
typedef enum th_handles {
    AUDIO_THREAD_HNDLE,
    AUDIO_PROD_THREAD_HNDLE,
    VIDEO_THREAD_HNDLE,
    VIDEO_PROD_THREAD_HNDLE,
} th_handles;

typedef enum ev_handles {
    AUDIO_PROD_EVENT_WAKE_HNDLE,
    VIDEO_PROD_EVENT_WAKE_HNDLE,
} ev_handles;

typedef struct frame {
    char  *compressed_data;
    size_t compressed_size;
    size_t uncompressed_size;
    size_t serial;
} frame;

/// @brief Player.
typedef struct player {
    frame         **video_rbuffer;
    s16_le         *audio_rbuffer;
    char           *gwpvbuffer; // Work buffer. VProducer.
    char           *gwcvbuffer; // Work buffer. VConsumer.
    atomic_bool_t   shutdown;
    atomic_bool_t   playing;
    atomic_bool_t   looping;
    atomic_bool_t   invalidated; // Any disruptive operation. (Seeking, console resizing)
    atomic_double_t main_clock;
    atomic_double_t volume;
    atomic_double_t seek_speed;
    atomic_size_t   serial; // Data versioning.
    atomic_size_t   aread_idx;
    atomic_size_t   awrite_idx;
    atomic_size_t   vread_idx;
    atomic_size_t   vwrite_idx;
    DWORD           active_threads;
    HANDLE         *th_hndles; // Use with `th_handles`.
    HANDLE         *ev_hndles; // Use with `ev_handles`.
    thread_data   **th_data;   // Use with `th_handle_idx`.
    media_mtdta    *media_mtdta;
} player;

/// @brief Media metadata.
typedef struct media_mtdta {
    WCHAR *media_path;
    double duration;
    size_t height;
    size_t width;
    bool   video_present;
    bool   audio_present;
} media_mtdta;

/// @brief Thread IDs.
/// @note Order is crucial to WaitForMultipleObjects(). Do not touch.
typedef enum thread_id {
    AUDIO_THREAD_ID,
    AUDIO_PROD_THREAD_ID,
    VIDEO_THREAD_ID,
    VIDEO_PROD_THREAD_ID
} thread_id;

/// @brief Thread data to be passed at creation.
typedef struct thread_data {
    player   *player;
    thread_id thread_id;
} thread_data;
