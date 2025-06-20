#ifndef TERMIPLAY_VECTOR
#define TERMIPLAY_VECTOR

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tpl_errors.h"

#define INIT_VEC_CAPACITY 4

/// @brief Resizable array.
typedef struct {
    void*  data;
    size_t capacity;
    size_t len;
    size_t idx_size;
} vec;

/// @brief Creates a heap-allocated vector.
/// @param idx_size sizeof() of an element.
/// @param vec_ptr Pointer.
/// @return Pointer to a heap-allocated vector.
static tpl_result vec_init(
    const size_t idx_size,
    vec**        vec_ptr
) {
    vec*       v    = NULL;
    tpl_result code = TPL_SUCCESS;
    if (vec_ptr == NULL) {
        code = TPL_RECEIVED_NULL;
        LOG_ERR(code);
        goto error;
    }
    // A pointer that isn't NULL might have existing data.
    if (*vec_ptr != NULL) {
        code = TPL_OVERWRITE;
        LOG_ERR(code);
        goto error;
    }
    if (idx_size == 0) {
        code = TPL_INVALID_ARGUMENT;
        LOG_ERR(code);
        goto error;
    }

    v = malloc(sizeof(vec));
    if (v == NULL) {
        code = TPL_ALLOC_FAILED;
        LOG_ERR(code);
        goto error;
    }

    v->len      = 0;
    v->capacity = INIT_VEC_CAPACITY;
    v->idx_size = idx_size;
    v->data     = NULL; // No-op when `free()`-ed. Allows easier clean-up in `error:`.

    if (INIT_VEC_CAPACITY > SIZE_MAX / v->idx_size) {
        code = TPL_OVERFLOW;
        LOG_ERR(code);
        goto error;
    }
    void* mem_block = malloc(v->capacity * v->idx_size);
    if (mem_block == NULL) {
        code = TPL_ALLOC_FAILED;
        LOG_ERR(code);
        goto error;
    }
    v->data = mem_block;
error:
    if (tpl_failed(code)) {
        if (v) {
            free(v->data);
            free(v);
            v = NULL;
        }
        return code;
    }
    *vec_ptr = v;
    return code;
}

/// @brief Random access.
/// @param vec Source vector.
/// @param idx vector index.
/// @param element A pointer that must point to a NULL pointer variable.
/// @return Return code.
static inline tpl_result vec_at(
    vec*         vec,
    const size_t idx,
    void**       element
) {
    if (element == NULL || vec == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        *element = NULL;
        return TPL_RECEIVED_NULL;
    }
    // A pointer that isn't NULL might have existing data.
    if (*element != NULL) {
        LOG_ERR(TPL_OVERWRITE);
        *element = NULL;
        return TPL_OVERWRITE;
    }
    if (vec->len <= idx) {
        LOG_ERR(TPL_INDEX_ERROR);
        *element = NULL;
        return TPL_INDEX_ERROR;
    }
    *element = (char*)vec->data + idx * vec->idx_size;
    return TPL_SUCCESS;
}

/// @brief Random access. Const variation.
/// @param vec Source vector.
/// @param idx vector index.
/// @return A pointer to the specified index of the source vector.
static inline tpl_result vec_at_const(
    const vec*   vec,
    const size_t idx,
    const void** element
) {
    if (element == NULL || vec == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        *element = NULL;
        return TPL_RECEIVED_NULL;
    }
    // A pointer that isn't NULL might have existing data.
    if (*element != NULL) {
        LOG_ERR(TPL_OVERWRITE);
        *element = NULL;
        return TPL_OVERWRITE;
    }
    if (vec->len <= idx) {
        LOG_ERR(TPL_INDEX_ERROR);
        *element = NULL;
        return TPL_INDEX_ERROR;
    }
    *element = (const char*)vec->data + idx * vec->idx_size;
    return TPL_SUCCESS;
}

/// @brief Resize vector utility. Does nothing when target is smaller than
/// initial capacity.
/// @param vec Source vector.
/// @param target Target capacity.
/// @return A boolean signifying success or failure.
static tpl_result vec_ensure_capacity(
    vec*         vec,
    const size_t target
) {
    if (vec == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (vec->capacity >= target) {
        return TPL_SUCCESS; // No-op.
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
    if (new_capacity == 0xFFFFFFFFFFFFFFFF) {
        return TPL_OVERFLOW;
    }
    new_capacity++;
    if (new_capacity > SIZE_MAX / vec->idx_size) {
        LOG_ERR(TPL_OVERFLOW);
        return TPL_OVERFLOW;
    }
    void* mem_block = realloc(vec->data, new_capacity * vec->idx_size);
    if (mem_block == NULL) {
        LOG_ERR(TPL_ALLOC_FAILED);
        return TPL_ALLOC_FAILED;
    }
    vec->data     = mem_block;
    vec->capacity = new_capacity;
    return TPL_SUCCESS;
}

#define vec_push(vec, element_ptr) vec_mulpush((vec), (element_ptr), 1)
/// @brief Multiple push to a vector.
/// @param vec Destination vector.
/// @param data Data to be passed. Assumed to be the same size as
/// `vec->idx_size`. MUST be the starting pointer. Undefined behavior otherwise.
/// @param data_count Number of elements to be pushed.
/// @return Return status.
static tpl_result vec_mulpush(
    vec*         vec,
    const void*  data,
    const size_t data_count
) {
    if (vec == NULL || data == NULL) {
        LOG_ERR(TPL_RECEIVED_NULL);
        return TPL_RECEIVED_NULL;
    }
    if (data_count == 0) {
        return TPL_SUCCESS;
    }
    if (data_count > SIZE_MAX - vec->len) {
        LOG_ERR(TPL_OVERFLOW);
        return TPL_OVERFLOW;
    }
    size_t new_len = vec->len + data_count;
    if (new_len > vec->capacity) {
        tpl_result realloc_result = vec_ensure_capacity(vec, new_len);
        if (tpl_failed(realloc_result)) {
            LOG_ERR(realloc_result);
            return realloc_result;
        }
    }
    memcpy((void*)((char*)vec->data + vec->len * vec->idx_size), data, data_count * vec->idx_size);
    vec->len = new_len;
    return TPL_SUCCESS;
}

/// @brief `free()` a heap-allocated vector, and set the pointer pointing to it
/// to NULL.
/// @param vec_ptr Pointer to the pointer of the target vector.
static inline void vec_destroy(vec** vec_ptr) {
    if (vec_ptr == NULL || *vec_ptr == NULL) {
        return;
    }
    free((*vec_ptr)->data);
    free(*vec_ptr);
    *vec_ptr = NULL;
}

#endif
