#pragma once

#include <windows.h>
#include <string>

namespace UIUtils {
    // Создает современный шрифт со сглаживанием
    HFONT CreateModernFont(int height, int weight = FW_NORMAL, bool italic = false);

    // Настраивает контрол (например, STATIC или EDIT) для красивого отображения
    void ApplySmoothStyle(HWND hControl);
}