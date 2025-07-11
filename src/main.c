#define MINIAUDIO_IMPLEMENTATION
#include "tl_app.h"
#include "tl_errors.h"
#include "tl_pch.h"
#include "tl_types.h"
#include "tl_utils.h"

int main() {
    tl_result     excv = TL_SUCCESS;
    int           argc = 0;
    const WCHAR  *wcmd_line = GetCommandLineW();
    const WCHAR **wargv = CommandLineToArgvW(wcmd_line, &argc);

    uint8_t sys_ret = 0;
    sys_ret |= system("cls");
    sys_ret |= system("ffmpeg -version 1>NUL 2>NUL");
    sys_ret |= system("ffprobe -version 1>NUL 2>NUL");
    CHECK(excv, sys_ret != 0, TL_DEP_NOT_FOUND, return excv);

    

    uint8_t set_ret = 0;
    set_ret |= SetConsoleOutputCP(CP_UTF8) ? 0 : 0x01;
    set_ret |= SetConsoleCP(CP_UTF8) ? 0 : 0x10;
    CHECK(excv, set_ret != 0, TL_CONSOLE_ERR, return excv);

    HANDLE stdouth = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE stdinh = GetStdHandle(STD_INPUT_HANDLE);
    CHECK(excv, stdinh == INVALID_HANDLE_VALUE || stdinh == NULL, TL_OS_ERR, return excv);
    CHECK(excv, stdouth == INVALID_HANDLE_VALUE || stdouth == NULL, TL_OS_ERR, return excv);

    DWORD stdin_defm = 0;
    DWORD stdout_defm = 0;

    uint8_t get_ret = 0;
    get_ret |= GetConsoleMode(stdouth, &stdout_defm) ? 0 : 0x01;
    get_ret |= GetConsoleMode(stdinh, &stdin_defm) ? 0 : 0x10;
    CHECK(excv, get_ret != 0, TL_CONSOLE_ERR, return excv);

    DWORD stdin_newm =
        stdin_defm & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    DWORD stdout_newm = stdout_defm & ~ENABLE_PROCESSED_OUTPUT;

    set_ret = 0;
    set_ret |= SetConsoleMode(stdinh, stdin_newm) ? 0 : 0x01;
    set_ret |= SetConsoleMode(stdouth, stdout_newm) ? 0 : 0x10;
    CHECK(excv, set_ret != 0, TL_CONSOLE_ERR, return excv);

    TRY(excv, player_exec(argc, wargv), break);

    SetConsoleMode(stdouth, stdout_defm);
    SetConsoleMode(stdinh, stdin_defm);
    return excv;
}