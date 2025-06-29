#include <Windows.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "tl_app.h"
#include "tl_errors.h"
#include "tl_test.h"

// #define TEST

int main() {
#ifndef TEST
    tl_result exit_code = TL_SUCCESS;
    WCHAR    *cmd_line = GetCommandLineW();
    int       argc;
    WCHAR   **argv = CommandLineToArgvW(cmd_line, &argc);

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    bool ffmpeg_exists = system("ffmpeg -version 2> NUL 1> NUL") == 0;
    CHECK(exit_code, !ffmpeg_exists, TL_FFMPEG_NOT_FOUND, return TL_FFMPEG_NOT_FOUND);
    TRY(exit_code, app_exec(argv[1]), return exit_code);
#else
    test_main();
#endif
}