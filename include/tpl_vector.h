#ifndef TERMIPLAY_VECTOR
#define TERMIPLAY_VECTOR

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h> // memcpy()

#define INIT_VEC_CAPACITY 4

/// @todo INTEGER OVERFLOW BUGS IN THIS FILE. Well it'll be mostly fine for regular use anyways.

/// @brief Resizable array.
typedef struct {
    void* data;
    size_t capacity;
    size_t len;
    size_t idx_size;
} vector;


/// @brief Creates a heap-allocated vector.
/// @param idx_size sizeof() of an element.
/// @return Pointer to a heap-allocated vector.
static inline vector* vec_create(size_t idx_size) {
    if (idx_size == 0) {
        return NULL;
    }
    vector* v = (vector*)malloc(sizeof(vector));
    if (v == NULL) {
        return NULL;
    }
    v->len = 0;
    v->capacity = INIT_VEC_CAPACITY;
    v->idx_size = idx_size;
    void* mem_block = malloc(v->capacity * v->idx_size);
    if (mem_block == NULL) {
        free(v);
        return NULL;
    }
    v->data = mem_block;
    return v;
}

/// @brief Random access. Returns NULL in case of out-of-range.
/// @param vec Source vector.
/// @param idx vector index.
/// @return A pointer to the specified index of the source vector.
static inline void* vec_at(
    vector* vec,
    size_t idx
) {
    if (vec == NULL || idx >= vec->len) {
        return NULL;
    }
    return (void*)((char*)vec->data + idx * vec->idx_size);
}

/// @brief Random access. Returns NULL in case of out-of-range. Const variation.
/// @param vec Source vector.
/// @param idx vector index.
/// @return A pointer to the specified index of the source vector.
static inline const void* vec_at_const(
    const vector* vec,
    size_t idx
) {
    if (vec == NULL || idx >= vec->len) {
        return NULL;
    }
    return (const void*)((char*)vec->data + idx * vec->idx_size);
}

/// @brief Resize vector utility. Does nothing when target is smaller than
/// initial capacity.
/// @param vec Source vector.
/// @param target Target capacity.
/// @return A boolean signifying success or failure.
static inline bool vec_ensure_capacity(
    vector* vec,
    size_t target
) {
    if (vec->capacity >= target || vec->idx_size == 0) {
        return true;
    }
    size_t new_capacity = target;
    if (INIT_VEC_CAPACITY > new_capacity) {
        new_capacity = INIT_VEC_CAPACITY;
    }

    // Bit-smearing.
    new_capacity--;
    new_capacity |= new_capacity >> 1;
    new_capacity |= new_capacity >> 2;
    new_capacity |= new_capacity >> 4;
    new_capacity |= new_capacity >> 8;
    new_capacity |= new_capacity >> 16;
    new_capacity |= new_capacity >> 32;
    new_capacity++;

    // Overflowed allocation.
    if (new_capacity > SIZE_MAX / vec->idx_size) {
        return false;
    }

    if (vec->data == NULL) {
        return false;
    }
    void* mem_block = realloc(vec->data, new_capacity * vec->idx_size);
    if (mem_block == NULL) {
        return false;
    }
    vec->data = mem_block;
    vec->capacity = new_capacity;
    return true;
}

/// @brief Multiple push to a vector.
/// @param vec Destination vector.
/// @param data Data to be passed. Assumed to be the same size as
/// `vec->idx_size`. MUST be the starting pointer. Undefined behavior otherwise.
/// @param data_count Number of elements to be pushed.
/// @return A boolean signifying success or failure.
static inline bool vec_mulpush(
    vector* vec,
    const void* data,
    size_t data_count
) {
    size_t new_len = vec->len + data_count;
    if (vec == NULL || data == NULL) {
        return false;
    }
    if (data_count == 0) {
        return true;
    }
    if (new_len > vec->capacity) {
        bool realloc_result = vec_ensure_capacity(vec, new_len);
        if (!realloc_result) {
            return false;
        }
    }
    memcpy((void*)((char*)vec->data + vec->len * vec->idx_size), data, data_count * vec->idx_size);
    vec->len = new_len;
    return true;
}

/// @brief `free()` a heap-allocated vector, and set the pointer pointing to it
/// to NULL.
/// @param vec_ptr Pointer to the pointer of the target vector.
static inline void vec_destroy(vector** vec_ptr) {
    if (vec_ptr != NULL && *vec_ptr != NULL) {
        free((*vec_ptr)->data);
        free(*vec_ptr);
        *vec_ptr = NULL;
    }
}

#endif
