#pragma once

#include <Windows.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct con_frame {
    char  *compressed_data;
    size_t compressed_bsize;
    size_t uncompressed_bsize;
    size_t flength; // Character cells that the frame occupies.
    size_t fwidth;  // Character cells that the frame occupies.
    size_t x_start;
    size_t y_start;
    double pts;
} con_frame;

typedef struct raw_frame {
    uint8_t *data;
    size_t   flength; // In pixels for the actual frame.
    size_t   fwidth;  // In pixels for the actual frame.
} raw_frame;

typedef struct con_bounds {
    size_t log_ln;
    size_t log_wdth;
    size_t cell_ln;
    size_t cell_wdth;
    size_t start_row; // For aspect ratio preservation.
    size_t start_col;
    size_t abs_conln;
    size_t abs_conwdth;
} con_bounds;

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
#define VBUFFER_BSIZE (V_FPS - 25) * sizeof(con_frame *)
#define ABUFFER_BSIZE A_SAMP_RATE / 5 * A_CHANNELS * sizeof(s16_le)
#define ASTREAM_BSIZE ABUFFER_BSIZE

#define MAXIMUM_RESOLUTION_WIDTH 1920
#define MAXIMUM_RESOLUTION_HEIGHT 1080
#define MAXIMUM_BUFFER_SIZE 3110400 // 2160 * 1440.
#define COLOR_CHANNEL_COUNT 1
#define BRAILLE_CHAR_DOT_LN 4
#define BRAILLE_CHAR_DOT_WDTH 2
#define BRAILLE_DOTS_PER_CHAR 8
#define BAYER_4X4_MATRIX_SIZE 16
#define BAYER_8X8_MATRIX_SIZE 64
#define BAYER_16X16_MATRIX_SIZE 256
#define FLOYD_STEINBERG_KERNEL_SIZE 4
#define HALFTONE_MATRIX_SIZE 16
#define SIERRA_LITE_KERNEL_SIZE 2
#define DTH_BLUE_MODES 4

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

/// @brief Key codes.
typedef enum key_code {
    NO_INPUT,
    SPACE,     // Play/Pause.
    L,         // Loop.
    ARR_UP,    // Volume up.
    ARR_DOWN,  // Volume down.
    ARR_LEFT,  // Seek forward.
    ARR_RIGHT, // Seek backward.
    M,         // Mute.
    G,         // Debug print.
    D,         // Switch dithering modes.
    R,         // Switch color modes.
    Q          // Shutdown.
} key_code;

/// @brief Dithering modes.
typedef enum dither_mode {
    DTH_BAYER_16X16,
    DTH_FLOYD_STEINBERG,
    DTH_HALFTONE,
    DTH_BLUE,
    DTH_BAYER_8X8,
    DTH_BAYER_4X4,
    DTH_SIERRA_LITE,
    DTH_THRESHOLDING,
    DTH_MODES
} dither_mode;

typedef enum color_mode {
    CLM_DARK_BLUE = 0b0001,
    CLM_DARK_GREEN = 0b0010,
    CLM_DARK_CYAN = 0b0011,
    CLM_DARK_RED = 0b0100,
    CLM_DARK_MAGENTA = 0b0101,
    CLM_DARK_YELLOW = 0b0110, // Can appear brown-ish.
    CLM_GRAY = 0b0111,
    CLM_DARK_GRAY = 0b1000,
    CLM_BLUE = 0b1001,
    CLM_GREEN = 0b1010,
    CLM_CYAN = 0b1011,
    CLM_RED = 0b1100,
    CLM_MAGENTA = 0b1101,
    CLM_YELLOW = 0b1110,
    CLM_WHITE = 0b1111,
    CLM_COUNT
} color_mode;

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
    con_frame     **video_rbuffer;
    s16_le         *audio_rbuffer;
    char           *gwpvbuffer; // Work buffer. VProducer.
    char           *gwcvbuffer; // Work buffer. VConsumer.
    atomic_bool_t   shutdown;
    atomic_bool_t   playing;
    atomic_bool_t   looping;
    atomic_bool_t   invalidated; // Any disruptive operation. (Seeking, console resizing)
    atomic_bool_t   muted;
    atomic_bool_t   debug_print;
    atomic_double_t main_clock;
    atomic_double_t volume;
    atomic_double_t seek_speed;
    atomic_size_t   dither_mode;
    atomic_size_t   color_mode;
    atomic_size_t   serial; // Data versioning.
    atomic_size_t   aread_idx;
    atomic_size_t   awrite_idx;
    atomic_size_t   vread_idx;
    atomic_size_t   vwrite_idx;
    atomic_size_t   ext_assets_ptr; // Extra data such as textures for dithering.
    DWORD           active_threads;
    HANDLE         *th_hndles; // Use with `th_handles`.
    HANDLE         *ev_hndles; // Use with `ev_handles`.
    thread_data   **th_data;   // Use with `th_handle_idx`.
    media_mtdta    *media_mtdta;
    SRWLOCK         srw_mclock;
} player;
