#ifndef TL_VIDEO
#define TL_VIDEO
#include <Windows.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "lz4.h"
#include "tl_errors.h"
#include "tl_types.h"

/// @brief Does a deep copy of all memory contents in src. Expects [compressed size(i),
/// uncompressed_size(i), data] format. State must not change during execution. Call in a lock.
/// @param src Source buffer.
/// @param dst Destination buffer.
tl_result dcopy_buffer_contents(
    const char **src,
    char       **dst
);

/// @brief Gets the new PTS based on a starting timestamp(clock).
/// @param pts_buf PTS buffer. Must hold VIDEO_FPS values. NULL is a no-op.
/// @param start_clock Starting timestamp. (SECONDS)
void get_new_pts(double *pts_buf, double start_clock);

/// @brief Executes video thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result video_thread_exec(thread_data *thdata);
#endif
