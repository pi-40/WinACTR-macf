#include <windows.h>

int main() {
    ShellExecuteA(NULL, "open", "C:\\Program Files\\WinACTR\\WinACTR.exe", NULL, NULL, SW_SHOWNORMAL);
    return 0;
}
