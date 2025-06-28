#include <Windows.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
int main() {
    wchar_t* cmd_line = GetCommandLineW();
    int argc;
    wchar_t** argv = CommandLineToArgvW(cmd_line, &argc);\

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}