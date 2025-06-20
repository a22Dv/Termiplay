#ifndef TERMIPLAY_TESTS
#define TERMIPLAY_TESTS

#include <stdbool.h>
#include <stdint.h>

typedef enum { TEST_PASSED = 1, TEST_FAILED = 0 } tpl_test_result;

typedef uint16_t tpl_test_fresult;
tpl_test_fresult start_testing();
tpl_test_result  test_vectors();
tpl_test_result  test_strings();
tpl_test_result  test_paths();
#endif
