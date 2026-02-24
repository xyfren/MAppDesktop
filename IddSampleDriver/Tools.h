#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>

namespace IddTools
{
    void CalculateCvtRbTiming(
        uint16_t width,
        uint16_t height,
        uint16_t refresh,
        uint16_t& h_blank,
        uint16_t& v_blank,
        uint16_t& h_fp,
        uint16_t& h_sync,
        uint16_t& v_fp,
        uint16_t& v_sync,
        uint32_t& pixel_clock_10khz)
    {
        // CVT Reduced Blanking v1

        h_fp = 48;
        h_sync = 32;
        uint16_t h_bp = 80;

        v_fp = 3;
        v_sync = 5;
        uint16_t v_bp = 23;

        h_blank = h_fp + h_sync + h_bp;
        v_blank = v_fp + v_sync + v_bp;

        uint32_t h_total = width + h_blank;
        uint32_t v_total = height + v_blank;

        uint32_t pixel_clock = h_total * v_total * refresh;

        pixel_clock_10khz = pixel_clock / 10000;
    }

    void GenerateEdid(BYTE* edid, uint16_t width, uint16_t height, uint16_t refreshRate)
    {
        // Обнуляем EDID
        ZeroMemory(edid, 128);

        // 1. Заголовок EDID (фиксированный)
        // 00 FF FF FF FF FF FF 00
        edid[0] = 0x00;
        edid[1] = 0xFF;
        edid[2] = 0xFF;
        edid[3] = 0xFF;
        edid[4] = 0xFF;
        edid[5] = 0xFF;
        edid[6] = 0xFF;
        edid[7] = 0x00;

        // 2. Manufacturer ID (1C EC = "HTC")
        edid[8] = 0x1C;   // H
        edid[9] = 0xEC;   // T + C

        // 3. Product Code (генерируем из разрешения)
        uint16_t productCode = (uint16_t)((width << 8) | (height & 0xFF));
        edid[10] = productCode & 0xFF;
        edid[11] = (productCode >> 8) & 0xFF;

        // 4. Serial Number (случайный)
        static uint32_t counter = 1;
        edid[12] = (counter >> 0) & 0xFF;
        edid[13] = (counter >> 8) & 0xFF;
        edid[14] = (counter >> 16) & 0xFF;
        edid[15] = (counter >> 24) & 0xFF;
        counter++;

        // 5. Дата производства (неделя 1, 2024 год)
        edid[16] = 0x01;  // Неделя 1
        edid[17] = 0x24;  // 2024 год (1990 + 0x24 = 2024)

        // 6. Версия EDID (1.3)
        edid[18] = 0x01;  // 1
        edid[19] = 0x03;  // 3

        // 7. Основные параметры дисплея
        edid[20] = 0x80;  // Digital input
        edid[21] = 0x00;  // Ширина в см (0 = не указано)
        edid[22] = 0x00;  // Высота в см (0 = не указано)
        edid[23] = 0x0A;  // Гамма 2.2

        // 8. Поддерживаемые функции
        edid[24] = 0x20;  // Стандартный sRGB цвет

        // 9. Цветовые координаты (стандартные sRGB)
        static const BYTE colorData[] = {
            0xEE, 0x91, 0xA3, 0x54, 0x4C, 0x99, 0x26, 0x0F, 0x50, 0x54
        };
        memcpy(&edid[25], colorData, sizeof(colorData));

        // 10. Established Timings (базовые режимы)
        edid[35] = 0x81;  // 800x600 и 1024x768 @ 60Hz
        edid[36] = 0x00;
        edid[37] = 0x00;

        // 11. Standard Timing Identification (просто заполняем нулями)
        // С 38 по 53 байт - стандартные тайминги
        for (int i = 38; i <= 53; i++) {
            edid[i] = 0x01;  // Безопасное значение
        }

        // 12. Detailed Timing Descriptor - наш режим (самое важное!)
        // Смещение 54, 18 байт

        uint16_t h_active = width;
        uint16_t v_active = height;

        uint16_t h_blank, v_blank;
        uint16_t h_fp, h_sync;
        uint16_t v_fp, v_sync;
        uint32_t pixel_clock_10khz;

        CalculateCvtRbTiming(
            width,
            height,
            refreshRate,
            h_blank,
            v_blank,
            h_fp,
            h_sync,
            v_fp,
            v_sync,
            pixel_clock_10khz
        );


        edid[54] = pixel_clock_10khz & 0xFF;
        edid[55] = (pixel_clock_10khz >> 8) & 0xFF;
        // Horizontal Active (ширина)
        edid[56] = h_active & 0xFF;                        // 0x80
        edid[57] = h_blank & 0xFF;                        // 0x18 (280)
        edid[58] = ((h_active >> 8) & 0x0F) << 4 |
            ((h_blank >> 8) & 0x0F);      // High byte (старшие 4 бита в следующем)

        edid[59] = v_active & 0xFF;                        // 0x38
        edid[60] = v_blank & 0xFF;                        // 0x2D (45)
        edid[61] = ((v_active >> 8) & 0x0F) << 4 |
            ((v_blank >> 8) & 0x0F);               // 0x04 << 4 | 0x00 = 0x40

        // Sync поля — можно оставить как было, или взять типичные значения
        edid[62] = 88 & 0xFF;          // H front porch
        edid[63] = 44 & 0xFF;          // H sync width
        edid[64] = 4 & 0xFF;          // V front porch
        edid[65] = 5 & 0x0F;          // V sync width

        // Размер изображения в мм (примерно 0.264 мм/пиксель → 23" монитор)
        uint16_t h_mm = (uint16_t)(width * 0.2646);
        uint16_t v_mm = (uint16_t)(height * 0.2646);
        edid[66] = h_mm & 0xFF;                    // Младшие 8 бит горизонтального размера
        edid[67] = v_mm & 0xFF;                    // Младшие 8 бит вертикального размера
        edid[68] = ((h_mm >> 8) & 0x0F) | (((v_mm >> 8) & 0x0F) << 4);  // Старшие 4 бита h_mm + старшие 4 бита v_mm

        // Остальные байты
        edid[69] = 0;  // Borders (горизонтальные)
        edid[70] = 0;  // Borders (вертикальные)
        edid[71] = 0x1A;  // Features bitmap

        edid[72] = 0x00;
        edid[73] = 0x00;

        // 13. Monitor Name (смещение 75)
        // Tag: Monitor Name (0xFC)
        edid[75] = 0x00;
        edid[76] = 0x00;
        edid[77] = 0x00;
        edid[78] = 0xFC;
        edid[79] = 0x00;

        // Имя монитора
        const char* name = "Virtual Display";
        for (int i = 0; i < 13; i++) {
            edid[80 + i] = (i < strlen(name)) ? name[i] : ' ';
        }

        // 14. Monitor Serial (смещение 93)
        // Tag: Serial Number (0xFF)
        edid[93] = 0x00;
        edid[94] = 0x00;
        edid[95] = 0x00;
        edid[96] = 0xFF;
        edid[97] = 0x00;

        // Серийный номер
        char serial[14];
        sprintf_s(serial, sizeof(serial), "SN-%08X", counter - 1);
        for (int i = 0; i < 13; i++) {
            edid[98 + i] = (i < strlen(serial)) ? serial[i] : ' ';
        }

        // 15. Extension flag (0 = нет расширений)
        edid[126] = 0x00;

        // 16. Checksum (последний байт)
        BYTE checksum = 0;
        for (int i = 0; i < 127; i++) {
            checksum += edid[i];
        }
        edid[127] = (BYTE)(256 - (int)checksum);
    }
}