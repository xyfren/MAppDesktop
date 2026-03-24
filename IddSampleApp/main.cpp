#include "AppCore.h"
#include "AppWindow.h"
#include "boost/locale.hpp"

extern "C" {
	// Этот символ заставляет драйвер NVIDIA переключиться на High Performance GPU
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000000;
}

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <iostream>
#include <iomanip>

void printAvailableEncoders() {
    void* i = nullptr;
    const AVCodec* codec = nullptr;

    std::cout << "=== Доступные видео-энкодеры в вашей системе ===" << std::endl;
    std::cout << std::left << std::setw(20) << "Имя" << "Описание" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    // Проходим по всем зарегистрированным кодекам
    while ((codec = av_codec_iterate(&i))) {
        // Нас интересуют только энкодеры и только видео
        if (av_codec_is_encoder(codec) && codec->type == AVMEDIA_TYPE_VIDEO) {
            std::cout << std::left << std::setw(20) << codec->name
                << codec->long_name << std::endl;
        }
    }
    std::cout << "================================================" << std::endl;
}

int __cdecl wmain(int argc, wchar_t *argv[])
{
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	boost::locale::generator gen;
	std::locale loc = gen("en_US.UTF-8");
	std::locale::global(loc);

    auto core = std::make_shared<AppCore>();
    core->start();

    AppWindow win;
    win.init(GetModuleHandle(nullptr), core);
    return win.runMessageLoop();
}