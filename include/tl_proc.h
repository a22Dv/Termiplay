#ifndef TL_PROC
#define TL_PROC

#include <stdbool.h>
#include "Windows.h"
#include "miniaudio.h"
#include "tl_errors.h"
#include "tl_types.h"

/// @brief Fills an audio buffer with s16_le samples. Fixed at runtime.
/// @param clock_start Where to begin sampling.
/// @param media_path Path to media file. Assumes existence of a valid audio streams.
/// @param buffer Assumes enough capacity for SAMPLE_RATE * CHANNELS * sizeof(int16_t) bytes.
/// @return Return code.
tl_result write_abuffer(
    const size_t  serial_at_call,
    player_state *pstate,
    const double  clock_start,
    const WCHAR  *media_path,
    int16_t      *buffer
);

/// @brief Fills a video buffer with LZ4 compressed ANSI frames.
/// @param clock_start Where to begin frame sampling.
/// @param media_path Path to media file. Assumes existence of a valid video stream.
/// @param buffer Assumes enough capacity to fit VIDEO_FPS heap-allocated char*.
/// @return Return code.
tl_result write_vbuffer_compressed(
    const size_t  serial_at_call,
    player_state *pstate,
    const double  clock_start,
    const WCHAR  *media_path,
    char        **buffer,
    const bool    default_charset
);
#endif