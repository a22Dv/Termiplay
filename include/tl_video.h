#ifndef TL_VIDEO
#define TL_VIDEO
#include <Windows.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "lz4.h"
#include "tl_errors.h"
#include "tl_types.h"

/// @brief Executes video thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result video_thread_exec(thread_data *thdata);
#endif
