#pragma once

#include <iostream>
#include <vector>
#include <stdio.h>
#include <thread>
#include <string>
#include <sstream>
#include <conio.h>
#include <wrl.h>
#include <wchar.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <swdevice.h>

#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include "../Common.h"
#include "../VideoBuffer.h"
#include "GpuDisplay.h"

class Monitor {
    
public:
    Monitor(const MonitorConfig& config, ID3D11Device* device, ID3D11DeviceContext* context);
    ~Monitor();

    bool Initialize(const wchar_t* frameReadyName,
                    const wchar_t* frameProcessedName,
                    const wchar_t* sharedMemoryName,
                    const wchar_t* sharedTextureName1,
                    const wchar_t* sharedTextureName2);

    void Run();

    std::thread& GetThread();
    
private:
    GpuDisplay* m_gDisplay;
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;

    MonitorConfig m_Config;
    VideoBuffer* m_pVideoBuffer;

    std::thread m_runThread;
    bool m_running;
    bool m_threadFinished;
};

class MonitorManager {
private:
    HANDLE m_hDriverDevice;
    std::vector<Monitor*> m_Monitors;
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
public:

    MonitorManager();

    bool Initialize();

    bool ConnectToDriver();

    bool AddMonitor(uint16_t monitorId, uint16_t width, uint16_t height, uint16_t byteDepth, uint16_t refreshRate);

    bool RemoveMonitor(uint16_t monitorId);

    DriverInfo GetDriverInfo();

    // Функция для FRAME_READY_EVENT со своим внутренним ID
    static std::wstring GetNextFrameReadyEvent() {
        static int frameReadyId = 0;
        frameReadyId++;
        return std::wstring(L"Global\\IddSampleFrameReady") + L"_" + std::to_wstring(frameReadyId);
    }

    // Функция для FRAME_PROCESSED_EVENT со своим внутренним ID
    static std::wstring  GetNextFrameProcessedEvent() {
        static int frameProcessedId = 0;
        frameProcessedId++;
        return std::wstring(L"Global\\IddSampleFrameProcessed") + L"_" + std::to_wstring(frameProcessedId);
    }

    // Функция для SHARED_MEMORY_NAME со своим внутренним ID
    static std::wstring GetNextSharedMemoryName() {
        static int sharedMemoryId = 0;
        sharedMemoryId++;
        return std::wstring(L"Global\\IddSampleSharedMemory") + L"_" + std::to_wstring(sharedMemoryId);
    }

    static std::wstring GetNextSharedTextureName(int sharedTextureId) {
        static int sharedTextureCommonId = 0;
        if (sharedTextureId % 2 == 0)
            sharedTextureCommonId++;
        return std::wstring(L"Global\\IddSampleSharedTexture_" + std::to_wstring(sharedTextureId) + L"_" + std::to_wstring(sharedTextureCommonId));
    }

private:
    bool WaitOpenDriver(DWORD intervalMs, DWORD maxTotalTimeMs);

};

