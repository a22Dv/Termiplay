#ifndef TERMIPLAY_FFMPEG_UTILS
#define TERMIPLAY_FFMPEG_UTILS

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
    swprintf(
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

#endif