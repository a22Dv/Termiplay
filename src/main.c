#include "tpl_errors.h"
#include "tpl_string.h"
#include "tpl_types.h"
#include "tpl_vector.h"
#include <stddef.h>
#include <assert.h>
 
int main(
    int argc,
    char** argv
) {
    // -----------------MUST DO--------------------
    // Make sure argv[1] is a valid path to a video
    // Make sure the video is in a supported format
    //---------------------------------------------

    // Set pipeline up with FFMPEG.
    // Set player to start
    // Enter while loop for player.
    // Check current console width

    // Resize frame accordingly while maintaining aspect ratio
    // Transform to ASCII to display
    // Set color if rgb is turned on
    // Play and sync while waiting for user input.
    // If user input, handle accordingly
    // Change character map, change grayscale/rgb, play/pause, fast-forward/backward by 10s

    printf("--- STARTING TESTS ---\n\n");

    // ====================================================================
    // Vector Tests
    // ====================================================================
    printf("--- Testing Generic Vector (vec) ---\n");
    {
        vec* v = NULL;

        printf("Test: vec_init success...\n");
        assert(vec_init(sizeof(int), &v) == TPL_SUCCESS);
        assert(v != NULL);
        assert(v->data != NULL);
        assert(v->len == 0);
        assert(v->capacity == INIT_VEC_CAPACITY);
        assert(v->idx_size == sizeof(int));
        printf("  [PASS]\n");

        printf("Test: vec_init failure cases...\n");
        assert(vec_init(sizeof(int), NULL) == TPL_RECEIVED_NULL);
        assert(vec_init(sizeof(int), &v) == TPL_OVERWRITE);
        vec* temp_v = NULL;
        assert(vec_init(0, &temp_v) == TPL_INVALID_ARGUMENT);
        assert(temp_v == NULL);
        printf("  [PASS]\n");

        printf("Test: vec_push and resizing...\n");
        for (int i = 0; i < 5; ++i) {
            int val = i * 10;
            assert(vec_push(v, &val) == TPL_SUCCESS);
        }
        assert(v->len == 5);
        assert(v->capacity == 8); // Resized from 4 to 8
        printf("  [PASS]\n");
        
        printf("Test: vec_mulpush...\n");
        int more_data[] = {50, 60, 70};
        assert(vec_mulpush(v, more_data, 3) == TPL_SUCCESS);
        assert(v->len == 8);
        assert(v->capacity == 8); // Should be exactly full
        printf("  [PASS]\n");

        printf("Test: vec_at and vec_at_const (with bug fix)...\n");
        for (size_t i = 0; i < v->len; ++i) {
            int* elem_ptr = NULL;
            const int* const_elem_ptr = NULL;
            
            assert(vec_at(v, i, (void**)&elem_ptr) == TPL_SUCCESS);
            assert(*elem_ptr == (int)i * 10);

            assert(vec_at_const(v, i, (const void**)&const_elem_ptr) == TPL_SUCCESS);
            assert(*const_elem_ptr == (int)i * 10);
        }
        printf("  [PASS]\n");

        printf("Test: vec_at boundary and error checks...\n");
        int* bad_ptr = NULL;
        // Test out of bounds (this is the critical test for the bug)
        assert(vec_at(v, v->len, (void**)&bad_ptr) == TPL_INDEX_ERROR);
        assert(bad_ptr == NULL);
        // Test way out of bounds
        assert(vec_at(v, 9000, (void**)&bad_ptr) == TPL_INDEX_ERROR);
        assert(bad_ptr == NULL);
        // Test NULL vector
        assert(vec_at(NULL, 0, (void**)&bad_ptr) == TPL_RECEIVED_NULL);
        assert(bad_ptr == NULL);
        printf("  [PASS]\n");
        
        printf("Test: vec_destroy...\n");
        vec_destroy(&v);
        assert(v == NULL);
        vec_destroy(&v); // Should be a no-op
        printf("  [PASS]\n");
    }
    printf("--- Vector Tests Complete ---\n\n");

    // ====================================================================
    // String Tests
    // ====================================================================
    printf("--- Testing String ---\n");
    {
        string* s = NULL;

        printf("Test: str_create...\n");
        assert(str_create(&s) == TPL_SUCCESS);
        assert(s != NULL);
        assert(s->buffer != NULL);
        assert(s->buffer->len == 0);
        assert(strcmp(str_c(s), "") == 0); // Is empty
        printf("  [PASS]\n");

        printf("Test: str_mulpush and concatenation...\n");
        assert(str_mulpush(s, "Hello") == TPL_SUCCESS);
        assert(s->buffer->len == 5);
        assert(s->buffer->capacity >= 6); // 5 chars + null
        assert(strcmp(str_c(s), "Hello") == 0);
        
        assert(str_mulpush(s, ", World!") == TPL_SUCCESS);
        assert(s->buffer->len == 13); // 5 + 8
        assert(s->buffer->capacity >= 14); // 13 chars + null
        assert(strcmp(str_c_const(s), "Hello, World!") == 0);
        printf("  [PASS]\n");

        printf("Test: Current string state...\n");
        printf("  Value: \"%s\"\n", str_c(s));
        printf("  Length: %zu\n", s->buffer->len);
        printf("  Capacity: %zu\n", s->buffer->capacity);

        printf("Test: str_destroy...\n");
        str_destroy(&s);
        assert(s == NULL);
        str_destroy(&s); // No-op
        printf("  [PASS]\n");
    }
    printf("--- String Tests Complete ---\n\n");
    
    // ====================================================================
    // Wide String Tests
    // ====================================================================
    printf("--- Testing Wide String (wstring) ---\n");
    {
        wstring* ws = NULL;

        printf("Test: wstr_create...\n");
        assert(wstr_create(&ws) == TPL_SUCCESS);
        assert(ws != NULL);
        assert(ws->buffer != NULL);
        assert(ws->buffer->len == 0);
        assert(wcscmp(wstr_c(ws), L"") == 0); // Is empty
        printf("  [PASS]\n");
        
        printf("Test: wstr_mulpush and concatenation...\n");
        assert(wstr_mulpush(ws,  L"Hello Wide") == TPL_SUCCESS);
        assert(ws->buffer->len == 10);
        assert(ws->buffer->capacity >= 11); // 10 chars + null
        assert(wcscmp(wstr_c(ws),  L"Hello Wide") == 0);
        assert(wstr_mulpush(ws,  L" World! €") == TPL_SUCCESS);
        assert(ws->buffer->len == 19); // 10 + 10
        assert(ws->buffer->capacity >= 21);
        assert(wcscmp(wstr_c_const(ws), L"Hello Wide World! €") == 0);
        printf("  [PASS]\n");

        printf("Test: Current wstring state...\n");
        // Using `wprintf` for wide strings
        wprintf(L"  Value: \"%ls\"\n", wstr_c(ws));
        printf("  Length: %zu\n", ws->buffer->len);
        printf("  Capacity: %zu\n", ws->buffer->capacity);

        printf("Test: wstr_destroy...\n");
        wstr_destroy(&ws);
        assert(ws == NULL);
        wstr_destroy(&ws); // No-op
        printf("  [PASS]\n");
    }
    printf("--- Wide String Tests Complete ---\n\n");
    
    printf("--- ALL TESTS PASSED ---\n");
    return 0;
}