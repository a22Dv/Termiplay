#pragma once

#include <Windows.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct frame {
    char  *compressed_data;
    size_t compressed_bsize;
    size_t uncompressed_bsize;
    size_t serial;
} frame;

typedef volatile LONG32 atomic_bool_t;
typedef volatile LONG64 atomic_size_t;
typedef volatile LONG64 atomic_double_t;
typedef int16_t         s16_le;


#define EXT_KEYCODE 224
#define ARR_UP_KEYC 72
#define ARR_DOWN_KEYC 80
#define ARR_LEFT_KEYC 75
#define ARR_RIGHT_KEYC 77
#define POLLING_RATE_MS 50
#define POLLING_RATE_S 0.05
#define MAX_THREADS 4
#define MAX_EVENTS 2
#define V_FPS 30
#define A_SAMP_RATE 48000
#define A_CHANNELS 2
#define V_FRAME_INTERVALS 0.0334
#define GBUFFER_BSIZE 500      // Generic buffer size.
#define GLBUFFER_BSIZE 1000    // Generic long buffer size.
#define GWVBUFFER_BSIZE 550000 // Generic work video buffer size. 550KB
#define VBUFFER_BSIZE 2 * V_FPS * sizeof(frame *)
#define ABUFFER_BSIZE A_SAMP_RATE * 2 * A_CHANNELS * sizeof(s16_le)


/// @brief Handle index.
/// @note Order is crucial to WaitForMultipleObjects(). Do not touch.
typedef enum th_handles {
    AUDIO_THREAD_HNDLE,
    AUDIO_PROD_THREAD_HNDLE,
    VIDEO_THREAD_HNDLE,
    VIDEO_PROD_THREAD_HNDLE,
} th_handles;

/// @brief Event handle index.
typedef enum ev_handles {
    AUDIO_PROD_EVENT_WAKE_HNDLE,
    VIDEO_PROD_EVENT_WAKE_HNDLE,
} ev_handles;

typedef enum key_code {
    NO_INPUT,
    SPACE,     // Play/Pause.
    L,         // Loop.
    ARR_UP,    // Volume up.
    ARR_DOWN,  // Volume down.
    ARR_LEFT,  // Seek forward.
    ARR_RIGHT, // Seek backward.
    M,         // Mute.
    Q          // Shutdown.
} key_code;

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
typedef enum thread_id {
    AUDIO_THREAD_ID,
    AUDIO_PROD_THREAD_ID,
    VIDEO_THREAD_ID,
    VIDEO_PROD_THREAD_ID
} thread_id;

typedef struct player player;

/// @brief Thread data to be passed at creation.
typedef struct thread_data {
    player   *player;
    thread_id thread_id;
} thread_data;

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
    atomic_bool_t   muted;
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
    SRWLOCK         srw_mclock;
} player;
