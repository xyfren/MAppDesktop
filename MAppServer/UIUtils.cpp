#include "UIUtils.h"

HFONT UIUtils::CreateModernFont(int pointSize, int weight, bool italic) {

    return CreateFontW(
        pointSize,                // Используем минус для точного соответствия размеру
        0, 0, 0,
        (weight > 0 ? weight : FW_NORMAL), // Если вес не задан, ставим 400
        italic, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_TT_PRECIS,             // Точный вывод TrueType шрифтов
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        VARIABLE_PITCH | FF_SWISS, // FF_SWISS идеально подходит для Segoe UI
        L"Segoe UI"
    );
}

void UIUtils::ApplySmoothStyle(HWND hControl) {
    static HFONT hDefaultFont = CreateModernFont(18); // Кэшируем шрифт
    SendMessage(hControl, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
}