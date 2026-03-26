#include "AppWindow.h"
#include "resource.h"
#include "AppCore.h"
#include <shellapi.h>

#define ID_TRAY_EXIT 40001

#define ID_MENU_WINDOW 40010
#define ID_CONSOLE_SHOW 40011
#define ID_CONSOLE_HIDE 40012

#define WM_TRAYICON (WM_USER + 1)

bool AppWindow::init(HINSTANCE hInst, std::shared_ptr<AppCore> core) {
    m_hInst = hInst;
    m_core = std::move(core);

    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = AppWindow::WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"MAppClass";
    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(
        0, wc.lpszClassName, L"MApp",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 200,
        nullptr, nullptr, hInst, this);

    m_hFont = UIUtils::CreateModernFont(20, 100, false);

    m_hStaticText = CreateWindowEx(
        0,
        L"STATIC",              // Класс окна для текста
        L"Сервер работает\nВерсия 0.1", // Сам текст
        WS_CHILD | WS_VISIBLE | SS_LEFT, // Стили: дочернее, видимое, выравнивание по левому краю
        10, 20,                 // X, Y координаты (относительно родителя)
        200, 40,                // Ширина и высота
        m_hwnd,             // Дескриптор вашего основного окна (AppWindow)
        NULL,
        m_hInst,
        NULL
    );

    // Чтобы изменить шрифт на системный (по умолчанию он выглядит старым):
    SendMessage(m_hStaticText, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    addTrayIcon();
    addWindowMenus();
    ShowWindow(m_hwnd, SW_SHOW);
    updateTitle();
    return true;
}

int AppWindow::runMessageLoop() {
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

void AppWindow::SetConsoleVisability(bool visible) {
    HWND hWnd = GetConsoleWindow();

    if (hWnd != NULL) {
        if (visible) {
            ShowWindow(hWnd, SW_SHOW); // Показать
        }
        else {
            ShowWindow(hWnd, SW_HIDE); // Скрыть
        }
    }
}

void AppWindow::addTrayIcon() {
    NOTIFYICONDATA nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(m_hInst, MAKEINTRESOURCE(IDI_APPICON));
    wcscpy_s(nid.szTip, L"MApp");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void AppWindow::removeTrayIcon() {
    NOTIFYICONDATA nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void AppWindow::addWindowMenus() {
    HMENU hWindowMenu = CreateMenu();
    HMENU hSubMenu = CreateMenu();
    AppendMenu(hSubMenu, MF_STRING, ID_CONSOLE_SHOW, L"Открыть консоль");
    AppendMenu(hSubMenu, MF_STRING, ID_CONSOLE_HIDE, L"Закрыть консоль");

    AppendMenu(hWindowMenu, MF_POPUP, (UINT_PTR) hSubMenu, L"Окно");

    SetMenu(m_hwnd,hWindowMenu);
}

void AppWindow::updateTitle() {
    if (!m_core) return;
    std::wstring title = L"MApp Server";
    SetWindowText(m_hwnd, title.c_str());
}

LRESULT AppWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE:
        ShowWindow(m_hwnd, SW_HIDE);
        return 0;

    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);

            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Выход");

            SetForegroundWindow(m_hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
            DestroyMenu(hMenu);
        }
        else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            ShowWindow(m_hwnd, SW_SHOW);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_CONSOLE_SHOW:
            SetConsoleVisability(true);
            return 0;

        case ID_CONSOLE_HIDE:
            SetConsoleVisability(false);
            return 0;

        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_DESTROY:
        if (m_core) m_core->stop();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppWindow* pThis = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<AppWindow*>(pCreate->lpCreateParams);

        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));

        if (pThis) pThis->m_hwnd = hwnd;
    }
    else {
        pThis = reinterpret_cast<AppWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

