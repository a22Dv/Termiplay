#pragma once

#include <stdbool.h>
#include "tl_types.h"

/// @brief Sets an atomic bool to a given value.
/// @param b Target atomic variable.
/// @param value Value to set to.
void set_atomic_bool(atomic_bool b, bool value);

/// @brief Gets the value held by an atomic bool.
/// @param b Target atomic variable.
/// @return Value of variable.
bool get_atomic_bool(atomic_bool b);

/// @brief Sets an atomic double to a given value.
/// @param dbl Target atomic variable.
/// @param value Value to set to.
void set_atomic_double(atomic_double dbl, double value);

/// @brief Gets the value held by an atomic double.
/// @param dbl Target atomic variable.
/// @return Value of variable.
double get_atomic_double(atomic_double dbl);

/// @brief Adds a given value to a target atomic double.
/// @param dbl Target atomic double.
/// @param addend Value to be added to variable.
void add_atomic_double(atomic_double dbl, double addend);

/// @brief Sets an atomic size_t to a given value.
/// @param st Target atomic variable.
/// @param value Value to be set to.
void set_atomic_size_t(atomic_size_t st, size_t value);

/// @brief Gets the value held by an atomic size_t.
/// @param st Target atomic variable.
/// @return Value of variable.
size_t get_atomic_size_t(atomic_size_t st);

/// @brief Adds a value to a target atomic size_t.
/// @param st Target atomic variable.
/// @param addend Value to be added to the variable.
void add_atomic_size_t(atomic_size_t st, size_t addend);

/// @brief Creates and allocates a `media_mtdta` to a NULL-ed out-parameter.
/// @param media_path Path to the media file.
/// @param out Out-parameter to hold created metadata.
/// @return Return code.
tl_result create_media_mtdta(const WCHAR* media_path, const media_mtdta** out);

/// @brief Creates and allocates a `thread_data` to a NULL-ed out-parameter.
/// @param out Out-parameter to hold created thread data.
/// @return Return code.
tl_result create_thread_data(thread_data** out);

/// @brief Creates and allocates a `player` to a NULL-ed out-parameter.
/// @param out Out-parameter to hold created player struct.
/// @return Return code.
tl_result create_player(player** out);

/// @brief Corresponding destroy function to free struct.
/// @param mtdta_ptr Address of pointer to metadata.
void destroy_media_mtdta(media_mtdta** mtdta_ptr);

/// @brief Corresponding destroy function to free struct.
/// @param thdta_ptr Address of pointer to thread data.
void destroy_thread_data(thread_data** thdta_ptr);

/// @brief Corresponding destroy function to free struct.
/// @param pl_ptr Address of pointer to player struct.
void destroy_player(player** pl_ptr);

