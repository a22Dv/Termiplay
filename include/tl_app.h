#pragma once
#include <Windows.h>
#include "tl_types.h"
#include "tl_errors.h"

/// @brief Start player execution.
/// @param argc Argument count.
/// @param wargv Wide argument vector.
/// @return Return code.
tl_result player_exec(const int argc, const WCHAR** wargv);

/// @brief Gets and translates user input as a `key_code` to an out parameter.
/// @param kc Pointer to key code location.
/// @return Return code.
void get_input(key_code *kc);

/// @brief Alters player state based on key code given.
/// @param pl Player.
/// @param kc Key code.
void process_input(player* pl, const key_code kc);