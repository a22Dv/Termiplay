#pragma once

#include "tl_errors.h"
#include "tl_types.h"

/// @brief Starts producer video thread execution.
/// @param data Thread data.
/// @return Thread exit code.
tl_result vpthread_exec(thread_data *data);

/// @brief Starts producer video thread execution.
/// @param data Thread data.
/// @return Thread exit code.
tl_result vcthread_exec(thread_data *data);