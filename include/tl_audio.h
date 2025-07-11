#pragma once
#include "tl_errors.h"
#include "tl_types.h"

/// @brief Starts audio producer thread execution.
/// @param data Thread data.
/// @return Thread exit code.
tl_result apthread_exec(thread_data *data);

/// @brief Starts audio consumer thread execution.
/// @param data Thread data.
/// @return Thread exit code.
tl_result acthread_exec(thread_data *data);