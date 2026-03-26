#pragma once
#include <atomic>
#include <thread>
#include <string>
#include "MApp.h"

class AppCore
{
public:
    AppCore() = default;
    ~AppCore();

    bool start();
    void stop();

private:
    void worker();

    MApp* m_pMApp = nullptr;

    std::thread m_thread;
};

