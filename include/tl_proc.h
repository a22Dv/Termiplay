#ifndef TL_PROC
#define TL_PROC

#include <Windows.h>
#include <process.h>
#include <stdbool.h>
#include <stdio.h>
#include "lz4.h"
#include "tl_errors.h"
#include "tl_types.h"
#include "tl_utils.h"

#define RGB_CHAR "▀"
#define BRAILLE_OFFSET "⠀"
#define WORKER_A1 0
#define WORKER_A2 1
#define WORKER_V1 2
#define WORKER_V2 3
#define COMMAND_BUFFER_SIZE 512

/// @brief Executes producer thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result proc_thread_exec(thread_data *thdata);

/// @brief Writes to a given audio buffer.
/// @param serial_at_call Data serial at call.
/// @param pstate Player state.
/// @param clock_start Clock start of decoding.
/// @param media_path Path to media file.
/// @param streams Stream bitmask.
/// @param buffer Buffer to write to.
/// @return Return code.
tl_result write_abuffer(
    const size_t  serial_at_call,
    player_state *pstate,
    const double  clock_start,
    const WCHAR  *media_path,
    const uint8_t streams,
    int16_t      *buffer
);

/// @brief Writes to a given audio buffer.
/// @param serial_at_call Data serial at call.
/// @param thdata Caller thread data.
/// @param clock_start Clock start of decoding.
/// @param media_path Path to media file.
/// @param streams Stream bitmask.
/// @param buffer Buffer to write to.
/// @param default_charset Whether to use the default character system or the other one.
/// @return Return code.
tl_result write_vbuffer_compressed(
    const size_t  serial_at_call,
    thread_data  *thdata,
    const double  clock_start,
    const WCHAR  *media_path,
    const uint8_t streams,
    char         *uncomp_fbuffer,
    char        **buffer,
    const bool    default_charset
);

/// @brief Executes the audio decoder helper thread.
/// @param worker_thdata Worker thread data.
/// @return Return code.
tl_result audio_helper_thread_exec(wthread_data *worker_thdata);

/// @brief Executes video decoder helper thread.
/// @param worker_thdata Worker thread data.
/// @return Return code.
tl_result video_helper_thread_exec(wthread_data *worker_thdata);

/// @brief Encodes a frame into an uncompressed char*.
/// @param frame_start Pointer to start of frame data.
/// @param height Height in pixels.
/// @param width in pixels.
/// @param color_channels Color channels per pixel.
/// @param default_charset Whether to use the default character set or no.
/// @param reference_frame  Passing NULL will make the function return a full keyframe. A delta
/// otherwise.
/// @param frame Where to store the result.
/// @return Return code.
tl_result frame_encode(
    uint8_t *frame_start,
    size_t   height,
    size_t   width,
    size_t   color_channels,
    bool     default_charset,
    uint8_t *reference_frame,
    char    *uncomp_fbuffer,
    char   **frame,
    size_t  *out_bytes_written
);

/// @brief Compresses a frame using LZ4.
/// @param frame Frame to compress.
/// @param compressed_out NULL-ed location to store compressed frame
/// @return Return code.
tl_result frame_compress(
    char   *frame,
    char  **compressed_out,
    size_t  frame_size_bytes
);

/// @brief
/// @param frame_start
/// @param height
/// @param width
/// @param reference_frame
/// @param uncomp_fbuffer
/// @param frame
/// @return
tl_result _frame_encode_braille(
    uint8_t *frame_start,
    size_t   height,
    size_t   width,
    uint8_t *reference_frame,
    char    *uncomp_fbuffer,
    char   **frame,
    size_t  *out_bytes_written
);

tl_result _frame_encode_rgb(
    uint8_t *frame_start,
    size_t   height,
    size_t   width,
    uint8_t *reference_frame,
    char    *uncomp_fbuffer,
    char   **frame,
    size_t  *out_bytes_written
);

/// @brief Retrieves a braille character based on two pixel values.
/// @param tp_v Top pixel value.
/// @param bot_v Bottom pixel value.
/// @return Braille character in a `wchar_t`.
wchar_t get_braille(
    uint8_t tp_v,
    uint8_t bot_v
);

#endif