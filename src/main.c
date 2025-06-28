#include <Windows.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

#define CHCP_UTF8 "chcp 65001"
int main() {
    wchar_t* cmd_line = GetCommandLineW();
    int wargc;
    wchar_t** wargv = CommandLineToArgvW(cmd_line, &wargc);
    wchar_t buffer[1024];
    swprintf(buffer, 1024, L"ffplay \"%ls\"", wargv[1]);
    wprintf(L"%ls\n", buffer);
    return _wsystem(buffer);
}