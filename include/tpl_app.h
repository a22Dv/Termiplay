#ifndef TERMIPLAY_APP
#define TERMIPLAY_APP

#include "tpl_errors.h"
#include "tpl_path.h"

/// @brief Starts main application execution. Expects UTF-8 char**.
/// @return Return code.
tpl_result start_execution(wpath* video_path);

#endif