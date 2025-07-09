#include "tl_errors.h"
#include "tl_pch.h"
#include "tl_types.h"

void set_atomic_bool(
    atomic_bool b,
    bool        value
) {}

bool get_atomic_bool(atomic_bool b) {}

void set_atomic_double(
    atomic_double dbl,
    double        value
) {}

double get_atomic_double(atomic_double dbl) {}
void   add_atomic_double(
      atomic_double dbl,
      double        addend
  ) {}

void set_atomic_size_t(
    atomic_size_t st,
    size_t        value
) {}

size_t get_atomic_size_t(atomic_size_t st) {}

void add_atomic_size_t(
    atomic_size_t st,
    size_t        addend
) {}

tl_result create_media_mtdta(
    const WCHAR        *media_path,
    const media_mtdta **out
) {}

tl_result create_thread_data(thread_data **out) {}

tl_result create_player(player **out) {}

void destroy_media_mtdta(media_mtdta **mtdta) {}

void destroy_thread_data(thread_data **thdta) {}

void destroy_player(player **pl) {}