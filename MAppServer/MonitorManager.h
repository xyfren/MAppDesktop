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
#include <functional>
#include <windows.h>
#include <swdevice.h>
#include <span>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include "../Common.h"
#include "../VideoBuffer.h"
#include "TimeProfiler.h"

class Monitor;

using SendFrameCallback = std::function<void(std::shared_ptr<Monitor> pMonitor, uint64_t frameId, uint32_t frameSize, uint32_t rowPitch, void* frameData)>;

class Monitor: public std::enable_shared_from_this<Monitor> {
public:
    Monitor(const MonitorConfig& config);
    ~Monitor();

    void setID3D11Device(ID3D11Device* device);
    void setID3D11DeviceContext(ID3D11DeviceContext* context);
	void setFrameCallback(SendFrameCallback sendFrameCallback);

    MonitorConfig GetConfig() const { return m_Config; }

    bool Initialize(const wchar_t* frameReadyName,
                    const wchar_t* frameProcessedName,
                    const wchar_t* sharedMemoryName,
                    const wchar_t* sharedTextureName1,
                    const wchar_t* sharedTextureName2);

    void Run();

    std::thread& GetThread();
    
private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stagingTexture1;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stagingTexture2;

    MonitorConfig m_Config;
    VideoBuffer* m_pVideoBuffer;

    std::thread m_runThread;
    bool m_running;
    bool m_threadFinished;

    SendFrameCallback m_sendFrameCallback;
};

class MonitorManager {
private:
    HANDLE m_hDriverDevice;
    std::vector<std::shared_ptr<Monitor>> m_Monitors;

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
public:

    MonitorManager();

    bool Initialize();

    bool ConnectToDriver();

    bool AddMonitor(std::shared_ptr<Monitor> pMonitor);

    bool RemoveMonitor(uint16_t monitorId);

    DriverInfo GetDriverInfo() const;

    static std::wstring GetNextFrameReadyEvent() {
        static int frameReadyId = 0;
        frameReadyId++;
        return std::wstring(L"Global\\IddSampleFrameReady") + L"_" + std::to_wstring(frameReadyId);
    }

    static std::wstring  GetNextFrameProcessedEvent() {
        static int frameProcessedId = 0;
        frameProcessedId++;
        return std::wstring(L"Global\\IddSampleFrameProcessed") + L"_" + std::to_wstring(frameProcessedId);
    }

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
    void SetDesktopExtendMode();
    std::pair<UINT32, UINT32> GetPrimaryMonitorResolution();
    void SetPrimaryMonitorResolution(UINT32 width, UINT32 height);
    bool WaitOpenDriver(DWORD intervalMs, DWORD maxTotalTimeMs);

};

