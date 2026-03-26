#pragma once
#include <windows.h>
#include <memory>

#include "UIUtils.h"

class AppCore;

class AppWindow {
public:
    bool init(HINSTANCE hInst, std::shared_ptr<AppCore> core);
    int runMessageLoop();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void addWindowMenus();
    void updateTitle();

    void addTrayIcon();
    void removeTrayIcon();

    void SetConsoleVisability(bool visible);

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInst = nullptr;
    std::shared_ptr<AppCore> m_core;
    
    // Элементы интерфейса
    HFONT m_hFont;

    HWND m_hStaticText;
    HMENU m_hWindowMenu;
};

