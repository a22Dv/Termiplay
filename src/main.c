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
    system("cls");

    // Console setup.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE stdin_h = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE stdout_h = GetStdHandle(STD_OUTPUT_HANDLE);
    CHECK(
        exit_code, stdout_h == NULL || stdout_h == INVALID_HANDLE_VALUE, TL_CONSOLE_ERROR,
        return exit_code
    );
    CHECK(
        exit_code, stdin_h == NULL || stdin_h == INVALID_HANDLE_VALUE, TL_CONSOLE_ERROR,
        return exit_code
    );

    
    DWORD prev_stdin_mode = 0;
    DWORD prev_stdout_mode = 0;
    CHECK(
        exit_code, !GetConsoleMode(stdout_h, &prev_stdout_mode), TL_CONSOLE_ERROR, return exit_code
    );
    CHECK(
        exit_code, !GetConsoleMode(stdin_h, &prev_stdin_mode), TL_CONSOLE_ERROR, return exit_code
    );

    DWORD cur_stdin_mode = prev_stdin_mode;
    DWORD cur_stdout_mode = prev_stdout_mode;
    cur_stdin_mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    cur_stdout_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    CHECK(exit_code, !SetConsoleMode(stdin_h, cur_stdin_mode), TL_CONSOLE_ERROR, return exit_code);
    CHECK(
        exit_code, !SetConsoleMode(stdout_h, cur_stdout_mode), TL_CONSOLE_ERROR, return exit_code
    );

    bool ffmpeg_exists = system("ffmpeg -version 2> NUL 1> NUL") == 0;
    CHECK(exit_code, !ffmpeg_exists, TL_FFMPEG_NOT_FOUND, return TL_FFMPEG_NOT_FOUND);

    // Entry point.
    TRY(exit_code, app_exec(argv[1]), return exit_code);

    CHECK(exit_code, !SetConsoleMode(stdin_h, prev_stdin_mode), TL_CONSOLE_ERROR, return exit_code);
    CHECK(
        exit_code, !SetConsoleMode(stdout_h, prev_stdout_mode), TL_CONSOLE_ERROR, return exit_code
    );
#else
    test_main();
#endif
}