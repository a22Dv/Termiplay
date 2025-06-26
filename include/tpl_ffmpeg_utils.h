#ifndef TERMIPLAY_FFMPEG_UTILS
#define TERMIPLAY_FFMPEG_UTILS

#define METADATA_COUNT 2

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "tpl_errors.h"
#include "tpl_path.h"
#include "tpl_types.h"

/// @brief Gives the number of streams in a media file.
/// @param ffprobe_output Output of FFprobe for a specified media file.
/// @param count Pointer to store results. Bit-packed. (Upper 4 bits) = Video, (Lower 4 bits) =
/// Audio.
/// @param len Length of the string.
/// @return Return code.
static tpl_result tpl_av_get_stream_count(
    const char*  ffprobe_output,
    uint8_t*     count,
    const size_t len
) {
    // "video"/"audio" have the same length.
    const size_t threshold = strlen("video");
    if (len < threshold) {
        *count = 0;
        LOG_ERR(TPL_INVALID_ARGUMENT);
        return TPL_INVALID_ARGUMENT;
    }
    uint8_t vstream_c = 0;
    uint8_t astream_c = 0;
    for (const char* ffp = ffprobe_output; *ffp != '\0'; ++ffp) {
        if (strncmp("video", ffp, threshold) == 0) {
            vstream_c += 1;
        } else if (strncmp("audio", ffp, threshold) == 0) {
            astream_c += 1;
        }
    }
    if (vstream_c > 15 || astream_c > 15) {
        *count = 0;
        LOG_ERR(TPL_UNSUPPORTED_STREAM);
        return TPL_UNSUPPORTED_STREAM;
    }
    *count = ((astream_c & 0x0F) | ((vstream_c & 0x0F) << 4));
    return TPL_SUCCESS;
}
/// @brief Checks if a file is a valid media file with 1 audio stream and 1 video stream.
/// @param file_wpath Path to the file.
/// @param result Pointer to result.
/// @return Return code.
static tpl_result tpl_av_file_stream_valid(
    wpath* file_wpath,
    bool*  result
) {
    if (result == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    // 1KB buffers.
    tpl_wchar command[1024];
    char      buffer[1024];
    FILE*     pipe;
    _snwprintf(
        command, _countof(command),
        L"ffprobe -v error -show_entries stream=codec_type -of "
        L"default=noprint_wrappers=1:nokey=1 \"%ls\"",
        wstr_c_const(file_wpath)
    );
    pipe = _wpopen(command, L"r");
    if (pipe == NULL) {
        *result = false;
        LOG_ERR(TPL_FAILED_TO_PIPE);
        return TPL_FAILED_TO_PIPE;
    }
    const size_t o_len = fread(buffer, sizeof(char), sizeof(buffer), pipe);
    buffer[o_len]      = '\0';
    int exit_code      = _pclose(pipe);

    // Either unsupported file or genuine failure, either way
    // *result will be false, the program wouldn't continue.
    if (exit_code != 0) {
        *result = false;
        return TPL_SUCCESS;
    }
    uint8_t stream_count = 0;
    tpl_av_get_stream_count(buffer, &stream_count, o_len);
    if (stream_count != 0x11) {
        *result = false;
    } else {
        *result = true;
    }
    return TPL_SUCCESS;
}

static tpl_result tpl_av_get_duration(
    wpath*  file_wpath,
    double* duration_buffer
) {
    IF_ERR_RET(file_wpath == NULL, TPL_RECEIVED_NULL);
    IF_ERR_RET(duration_buffer == NULL, TPL_RECEIVED_NULL);

    size_t    total_bytes_read = 0;
    tpl_wchar cmd_buffer[1024];
    char      result[128];
    FILE*     pipe = NULL;

    memset(result, 0, sizeof(result));

    _snwprintf(
        cmd_buffer, 1024,
        L"ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "
        L"\"%ls\"",
        wstr_c(file_wpath)
    );
    pipe = _wpopen(cmd_buffer, L"r");
    IF_ERR_RET(pipe == NULL, TPL_FAILED_TO_PIPE);
    char* result_ptr     = fgets(result, sizeof(result), pipe);
    int   proc_exit_code = _pclose(pipe);
    IF_ERR_RET(proc_exit_code != 0, TPL_PIPE_ERROR);
    IF_ERR_RET(result_ptr == NULL, TPL_PIPE_ERROR);
    *duration_buffer = strtod(result, NULL);
    return TPL_SUCCESS;
}

static tpl_result tpl_av_get_video_metadata(
    wpath*    video_wpath,
    uint16_t* video_height,
    uint16_t* video_width
) {
    size_t    total_bytes_read = 0;
    tpl_wchar cmd_buffer[1024];
    char      result[512];
    FILE*     pipe = NULL;

    memset(result, 0, sizeof(result));
    _snwprintf(
        cmd_buffer, 1024,
        L"ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of "
        L"default=noprint_wrappers=1:nokey=1 "
        L"\"%ls\"",
        wstr_c(video_wpath)
    );
    pipe = _wpopen(cmd_buffer, L"r");
    IF_ERR_RET(pipe == NULL, TPL_FAILED_TO_PIPE);
    char*  sentinel   = result; // result is used to start the loop with a non-NULL value.
    size_t chars_read = 0;
    while (sentinel != NULL) {
        sentinel   = fgets(result + chars_read, 512, pipe);
        chars_read = strlen(result);
    }
    int exit_code = _pclose(pipe);
    IF_ERR_RET(exit_code != 0, TPL_PIPE_ERROR);

    char*  metadata[METADATA_COUNT] = {NULL, NULL};
    size_t elem_idx                 = 0;
    bool   first_char               = true;
    for (char* chp = result; *chp != '\0'; ++chp) {
        if (first_char && elem_idx < METADATA_COUNT) {
            metadata[elem_idx] = chp;
            elem_idx += 1;
            first_char = false;
        }
        // Framerates are given in fractions. Treated as separate numbers
        if (*chp == '\n' | *chp == '/') {
            *chp       = 0x0;
            first_char = true;
        }
    }

    uint32_t lvideo_width  = strtoul(metadata[0], NULL, 10);
    uint32_t lvideo_height = strtoul(metadata[1], NULL, 10);

    *video_height = (uint16_t)lvideo_height;
    *video_width = (uint16_t)lvideo_width;
    return TPL_SUCCESS;
}
#endif