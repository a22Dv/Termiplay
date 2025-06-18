#define DEBUG
// #define TESTING

#ifndef TESTING
#include "tpl_app.h"
#include "tpl_errors.h"
#include "tpl_path.h"
#include "tpl_string.h"
#else
#include <stdio.h>
#include "tpl_tests.h"
#endif

int wmain(
    int argc,
    wchar_t** argv
) {
#ifndef TESTING
    if (argc != 2) {
        LOG_ERR(TPL_INVALID_ARGUMENT);
        fprintf(stderr, "Expected 2 arguments. Got %i.\n", argc);
        return TPL_INVALID_ARGUMENT;
    }
    wpath* video_path = NULL;
    tpl_result init_call = wpath_init(&video_path, argv[1]);
    if (tpl_failed(init_call)) {
        LOG_ERR(init_call);
        fprintf(stderr, "%s\n", err_str(init_call));
        return init_call;
    }
    tpl_result exec_result = start_execution(video_path);
    if (tpl_failed(exec_result)) {
        LOG_ERR(exec_result);
        return exec_result;
    }
    return TPL_SUCCESS;
#else
    tpl_test_fresult testing_result = start_testing();
    uint8_t tests_passed            = (testing_result & 0xFF00) >> 8;
    uint8_t tests_failed            = (testing_result & 0x00FF);
    fprintf(
        stderr, "%u tests passed. %u tests failed. %u tests conducted.", tests_passed, tests_failed,
        tests_passed + tests_failed
    );
#endif
}