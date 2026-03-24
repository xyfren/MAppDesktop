#pragma once
#include <windows.h>
#include <memory>

class AppCore;

class AppWindow {
public:
    bool init(HINSTANCE hInst, std::shared_ptr<AppCore> core);
    int runMessageLoop();

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInst = nullptr;
    std::shared_ptr<AppCore> m_core;

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    void addTrayIcon();
    void removeTrayIcon();
    void updateTitle();
};

