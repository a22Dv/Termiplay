#pragma once

#include "tl_errors.h"
#include "tl_types.h"

#define CHECK(excv, fail_expr, err, action)                                                        \
    do {                                                                                           \
        if (fail_expr) {                                                                           \
            LOG_ERR(err);                                                                          \
            excv = err;                                                                            \
            action;                                                                                \
        }                                                                                          \
    } while (0)

#define TRY(excv, fcall, action)                                                                   \
    do {                                                                                           \
        excv = fcall;                                                                              \
        if (excv != TL_SUCCESS) {                                                                  \
            LOG_ERR(excv);                                                                         \
            action;                                                                                \
        }                                                                                          \
    } while (0)

/// @brief For byte reinterpretation.
union double_l64 {
    double d;
    LONG64 l64;
};

/// @brief Sets an atomic bool to a given value.
/// @param b Target atomic variable.
/// @param value Value to set to.
void set_atomic_bool(
    atomic_bool_t *b,
    bool           value
);

/// @brief Gets the value held by an atomic bool.
/// @param b Target atomic variable.
/// @return Value of variable.
bool get_atomic_bool(atomic_bool_t *b);

/// @brief Flips an atomic bool. Internally uses a XOR flip.
/// @param b Target atomic variable
void flip_atomic_bool(atomic_bool_t *b);

/// @brief Sets an atomic double to a given value.
/// @param dbl Target atomic variable.
/// @param value Value to set to.
void set_atomic_double(
    atomic_double_t *dbl,
    double           value
);

/// @brief Gets the value held by an atomic double.
/// @param dbl Target atomic variable.
/// @return Value of variable.
double get_atomic_double(atomic_double_t *dbl);

/// @brief Adds a given value to a target atomic double.
/// @param dbl Target atomic double.
/// @param addend Value to be added to variable.
void add_atomic_double(
    atomic_double_t *dbl,
    double           addend
);

/// @brief Sets an atomic size_t to a given value.
/// @param st Target atomic variable.
/// @param value Value to be set to.
void set_atomic_size_t(
    atomic_size_t *st,
    size_t         value
);

/// @brief Gets the value held by an atomic size_t.
/// @param st Target atomic variable.
/// @return Value of variable.
size_t get_atomic_size_t(atomic_size_t* st);

/// @brief Adds a value to a target atomic size_t.
/// @param st Target atomic variable.
/// @param addend Value to be added to the variable.
void add_atomic_size_t(
    atomic_size_t *st,
    size_t         addend
);

/// @brief Creates and allocates a `media_mtdta` to a NULL-ed out-parameter.
/// @param media_path Path to the media file.
/// @param out Out-parameter to hold created metadata.
/// @return Return code.
tl_result create_media_mtdta(
    const WCHAR        *media_path,
    const media_mtdta **out
);

/// @brief Creates and allocates a `media_mtdta` to a NULL-ed out-parameter.
/// @param pl Player struct.
/// @param id Thread ID.
/// @param out Out-parameter to hold created thread data.
/// @return Return code.
tl_result create_thread_data(
    player         *pl,
    const thread_id id,
    thread_data   **out
);

// Forward declarations for dispatcher.

tl_result vpthread_exec(thread_data *data);
tl_result vcthread_exec(thread_data *data);
tl_result apthread_exec(thread_data *data);
tl_result acthread_exec(thread_data *data);

/// @brief Thread dispatcher function.
/// @param data Thread data.
/// @return Thread exit code.
unsigned int _stdcall thread_dispatcher(void *data);

/// @brief Creates and allocates a `player` to a NULL-ed out-parameter.
/// @param media_path Path to media file to play.
/// @param out Out-parameter to hold created player struct.
/// @return Return code.
tl_result create_player(
    const WCHAR *media_path,
    player     **out
);

/// @brief Copies a raw frame to a specified destination.
/// @param src Source raw frame.
/// @param dst_out Destination of the copy.
/// @return Return code.
tl_result copy_rawframe(raw_frame* src, raw_frame** dst_out);

/// @brief Corresponding destroy function to free struct.
/// @param mtdta_ptr Address of pointer to metadata.
void destroy_media_mtdta(media_mtdta **mtdta_ptr);

/// @brief Corresponding destroy function to free struct.
/// @param thdta_ptr Address of pointer to thread data.
/// @note Does not take ownership and thus won't free internal `media_mtdta*`.
void destroy_thread_data(thread_data **thdta_ptr);

/// @brief Corresponding destroy function to free struct.
/// @param pl_ptr Address of pointer to player struct.
/// @note Heavily relies on the operation order by create_player. Do not change on it's own.
void destroy_player(player **pl_ptr);

/// @brief Corresponding destroy function to free struct.
/// @param frame_ptr Address of pointer to frame struct.
void destroy_conframe(con_frame **frame_ptr);

/// @brief Corresponding destroy function to free struct.
/// @param frame_ptr Address of pointer to frame struct.
void destroy_rawframe(raw_frame **frame_ptr);

/// @brief Prints playback information to console.
/// @param pl Player struct.
void playback_stats(player *pl);

/// @brief Prints player state to console.
/// @param pl Player struct.
void state_print(player* pl);