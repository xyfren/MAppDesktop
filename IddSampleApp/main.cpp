#include <iostream>
#include <iomanip>
#include <ShellScalingApi.h>
#pragma comment(lib, "Shcore.lib")

#include "AppCore.h"
#include "AppWindow.h"
#include "boost/locale.hpp"

extern "C" {
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000000; // Этот символ заставляет драйвер NVIDIA переключиться на High Performance GPU
}

bool IsRunAsAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD dwSize;
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
            fIsRunAsAdmin = elevation.TokenIsElevated;
        }
    }
    if (hToken) CloseHandle(hToken);
    return fIsRunAsAdmin;
}

void CreateDebugConsole() {
    if (AllocConsole()) {
        FILE* fDummy;
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
        freopen_s(&fDummy, "CONIN$", "r", stdin);

        SetConsoleTitle(L"Debug Console");

        auto hConsole = GetConsoleWindow();
        
        if (hConsole) {
            HMENU hMenu = GetSystemMenu(hConsole, FALSE);
            if (hMenu) {
                DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
            }
        }
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    CreateDebugConsole();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::locale::global(locale("en_US.UTF-8"));

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED);

    if (!IsRunAsAdmin()) {
        MessageBoxW(nullptr,
            L"Для работы приложения требуются права администратора. Пожалуйста, запустите программу от имени администратора.",
            L"Ошибка доступа",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    auto core = std::make_shared<AppCore>();
    core->start();

    AppWindow win;
    win.init(hInst, core);
    return win.runMessageLoop();
}