#define DEBUG
// #define TESTING

#ifndef TESTING
#include "tpl_app.h"
#include "tpl_errors.h"
#else
#include <stdio.h>
#include "tpl_tests.h"
#endif

int main(
    int argc,
    char** argv
) {
#ifndef TESTING
    tpl_result exec_result = start_execution(argc, argv);
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