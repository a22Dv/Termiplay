#ifndef TL_AUDIO
#define TL_AUDIO

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "miniaudio.h"
#include "tl_errors.h"
#include "tl_types.h"

/// @brief Executes audio thread.
/// @param thdata Thread data.
/// @return Thread exit code.
tl_result audio_thread_exec(thread_data *thdata);

void callback(
    ma_device  *pDevice,
    void       *pOutput,
    const void *pInput,
    ma_uint32   frameCount
);

#endif