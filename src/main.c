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

    // Console setup.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE stdin_h = GetStdHandle(STD_INPUT_HANDLE);
    CHECK(
        exit_code, stdin_h == NULL || stdin_h == INVALID_HANDLE_VALUE, TL_CONSOLE_ERROR,
        return exit_code
    );
    DWORD prev_cmode = 0;
    CHECK(exit_code, !GetConsoleMode(stdin_h, &prev_cmode), TL_CONSOLE_ERROR, return exit_code);
    DWORD current_cmode = prev_cmode;
    current_cmode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    CHECK(exit_code, !SetConsoleMode(stdin_h, current_cmode), TL_CONSOLE_ERROR, return exit_code);
    bool ffmpeg_exists = system("ffmpeg -version 2> NUL 1> NUL") == 0;
    CHECK(exit_code, !ffmpeg_exists, TL_FFMPEG_NOT_FOUND, return TL_FFMPEG_NOT_FOUND);
    TRY(exit_code, app_exec(argv[1]), return exit_code);
    CHECK(exit_code, !SetConsoleMode(stdin_h, prev_cmode), TL_CONSOLE_ERROR, return exit_code);
#else
    test_main();
#endif
}