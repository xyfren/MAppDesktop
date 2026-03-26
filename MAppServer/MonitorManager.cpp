#include "MonitorManager.h"


#pragma region MonitorManager

MonitorManager::MonitorManager() {
    m_hDriverDevice = nullptr;
}

bool MonitorManager::Initialize() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL selectedFeatureLevel;

    HRESULT hr = D3D11CreateDevice(
        nullptr,                      // default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_device,
        &selectedFeatureLevel,
        &m_context
    );
    if (FAILED(hr)) return false;

    if (!ConnectToDriver()) {
        printf("ConnectToDriver failed.\n");
        printf("PLEASE INSTALL THE DRIVER!!!\n");
        return false;
    }
    return true;
}

bool MonitorManager::ConnectToDriver() {
    return WaitOpenDriver(200, 10000);
}

bool MonitorManager::AddMonitor(std::shared_ptr<Monitor> pMonitor) {
    auto prevPrimaryRes = GetPrimaryMonitorResolution();

    pMonitor->setID3D11Device(m_device.Get());
    pMonitor->setID3D11DeviceContext(m_context.Get());

    m_Monitors.push_back(pMonitor);

    wchar_t frameReadyName[128] = {};
    wchar_t frameProcessedName[128] = {};
    wchar_t sharedMemoryName[128] = {};
    wchar_t sharedTextureName1[128] = {};
    wchar_t sharedTextureName2[128] = {};

    swprintf_s(frameReadyName, GetNextFrameReadyEvent().c_str());
    swprintf_s(frameProcessedName, GetNextFrameProcessedEvent().c_str());
    swprintf_s(sharedMemoryName, GetNextSharedMemoryName().c_str());
    swprintf_s(sharedTextureName1, GetNextSharedTextureName(1).c_str());
    swprintf_s(sharedTextureName2, GetNextSharedTextureName(2).c_str());

    if (!pMonitor->Initialize(frameReadyName, frameProcessedName, sharedMemoryName, sharedTextureName1, sharedTextureName2)) {
        printf("Monitor initialization failed.\n");
        return false;
    }
    printf("Monitor initialized successfully.\n");

    CreateMonitorRequest request = {};
    request.config = pMonitor->GetConfig();
    wcsncpy_s(request.frameReadyName, frameReadyName, _TRUNCATE);
    wcsncpy_s(request.frameProcessedName, frameProcessedName, _TRUNCATE);
    wcsncpy_s(request.sharedMemoryName, sharedMemoryName, _TRUNCATE);
    wcsncpy_s(request.sharedTextureName1, sharedTextureName1, _TRUNCATE);
    wcsncpy_s(request.sharedTextureName2, sharedTextureName2, _TRUNCATE);

    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        m_hDriverDevice,
        IOCTL_IDD_ADD_MONITOR,
        &request, sizeof(request),
        NULL, 0,
        &bytesReturned, NULL
    );

    if (!result) {
        DWORD err = GetLastError();
        printf("DeviceIoControl failed. Error: %d (0x%x)\n", err, err);
        return false;
    }
    printf("DeviceIoControl succeeded. Bytes returned: %lu\n", bytesReturned);

    SetDesktopExtendMode();
    if (prevPrimaryRes.first != 0 && prevPrimaryRes.second)
        SetPrimaryMonitorResolution(prevPrimaryRes.first, prevPrimaryRes.second);

    return true;
}

bool MonitorManager::RemoveMonitor(uint16_t monitorId) {
    if (m_Monitors.size() <= monitorId) return false;
    uint16_t id = monitorId;
    DWORD bytesReturned;
    m_Monitors.erase(m_Monitors.begin() + monitorId);
    BOOL result = DeviceIoControl(
        m_hDriverDevice,
        IOCTL_IDD_REMOVE_MONITOR,
        &id, sizeof(id),
        NULL, 0,
        &bytesReturned, NULL
    );
    if (!result) {
        DWORD err = GetLastError();
        printf("DeviceIoControl failed. Error: %d (0x%x)\n", err, err);
        return false;
    }

    printf("DeviceIoControl succeeded. Bytes returned: %lu\n", bytesReturned);
    return true;
}

DriverInfo MonitorManager::GetDriverInfo() const {
    DriverInfo info = {};
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        m_hDriverDevice,
        IOCTL_IDD_GET_INFO,
        NULL, 0,
        &info, sizeof(info),
        &bytesReturned, NULL
    );
    if (!result) {
        DWORD err = GetLastError();
        printf("DeviceIoControl failed. Error: %d (0x%x)\n", err, err);
    }

    printf("DeviceIoControl succeeded. Bytes returned: %lu\n", bytesReturned);
    return info;
}

bool  MonitorManager::WaitOpenDriver(DWORD intervalMs, DWORD maxTotalTimeMs)
{
    ULONGLONG startTime = GetTickCount64();
    DWORD lastError = 0;

    while (true)
    {
        m_hDriverDevice = CreateFileW(
            L"\\\\.\\IddSampleDriver",
            GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING, 0, NULL
        );

        if (m_hDriverDevice != INVALID_HANDLE_VALUE)
        {
            return true;
        }

        lastError = GetLastError();

        ULONGLONG elapsed = GetTickCount64() - startTime;
        if (elapsed >= maxTotalTimeMs)
        {
            printf("Elapsed: [%d]\n", elapsed);
            break;
        }

        if (lastError != ERROR_FILE_NOT_FOUND &&
            lastError != ERROR_PATH_NOT_FOUND &&
            lastError != ERROR_DEVICE_NOT_CONNECTED &&
            lastError != ERROR_GEN_FAILURE &&
            lastError != 433L)
        {
            printf("Last error: [%d]\n", lastError);
            break;
        }

        // How much time remains
        DWORD remain = (DWORD)(maxTotalTimeMs - elapsed);
        DWORD sleepTime = (remain < intervalMs) ? remain : intervalMs;

        if (sleepTime == 0) break;

        Sleep(sleepTime);
    }
    return false;
}

void MonitorManager::SetDesktopExtendMode() {
    SetDisplayConfig(0, NULL, 0, NULL, SDC_TOPOLOGY_EXTEND | SDC_APPLY);
}

std::pair<UINT32, UINT32> MonitorManager::GetPrimaryMonitorResolution() {
    UINT32 numPathArrayElements = 0;
    UINT32 numModeInfoArrayElements = 0;

    // 1. Get buffer sizes for active paths
    LONG result = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, &numModeInfoArrayElements);
    if (result != ERROR_SUCCESS) {
        return { 0, 0 };
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> pathArray(numPathArrayElements);
    std::vector<DISPLAYCONFIG_MODE_INFO> modeInfoArray(numModeInfoArrayElements);

    // 2. Query current system configuration
    result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, pathArray.data(),
        &numModeInfoArrayElements, modeInfoArray.data(), nullptr);

    if (result != ERROR_SUCCESS) {
        return { 0, 0 };
    }

    // 3. Find the primary monitor (at coordinates 0,0)
    for (UINT32 i = 0; i < numPathArrayElements; i++) {
        UINT32 modeIdx = pathArray[i].sourceInfo.modeInfoIdx;

        // Validate index and type (must be Source Mode)
        if (modeIdx < numModeInfoArrayElements &&
            modeInfoArray[modeIdx].infoType == DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE) {

            // Primary monitor is always at (0,0)
            if (modeInfoArray[modeIdx].sourceMode.position.x == 0 &&
                modeInfoArray[modeIdx].sourceMode.position.y == 0) {

                return {
                    modeInfoArray[modeIdx].sourceMode.width,
                    modeInfoArray[modeIdx].sourceMode.height
                };
            }
        }
    }

    // Primary monitor not found (unlikely)
    return { 0, 0 };
}
void MonitorManager::SetPrimaryMonitorResolution(UINT32 width, UINT32 height) {
    UINT32 numPathArrayElements = 0;
    UINT32 numModeInfoArrayElements = 0;

    LONG result = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, &numModeInfoArrayElements);
    if (result != ERROR_SUCCESS) return;

    std::vector<DISPLAYCONFIG_PATH_INFO> pathArray(numPathArrayElements);
    std::vector<DISPLAYCONFIG_MODE_INFO> modeInfoArray(numModeInfoArrayElements);

    result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, pathArray.data(),
        &numModeInfoArrayElements, modeInfoArray.data(), nullptr);

    if (result != ERROR_SUCCESS) return;

    for (UINT32 i = 0; i < numPathArrayElements; i++) {
        UINT32 modeIdx = pathArray[i].sourceInfo.modeInfoIdx;

        if (modeIdx < numModeInfoArrayElements &&
            modeInfoArray[modeIdx].infoType == DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE) {

            // Primary monitor is at (0,0)
            if (modeInfoArray[modeIdx].sourceMode.position.x == 0 &&
                modeInfoArray[modeIdx].sourceMode.position.y == 0) {

                modeInfoArray[modeIdx].sourceMode.width = width;
                modeInfoArray[modeIdx].sourceMode.height = height;

                printf("Setting primary monitor resolution to %ux%u.\n", width, height);
            }
        }

        // Reset refresh rate so the system chooses it (prevents error 87)
        pathArray[i].targetInfo.refreshRate.Numerator = 0;
        pathArray[i].targetInfo.refreshRate.Denominator = 0;
    }
    result = SetDisplayConfig(
        numPathArrayElements, pathArray.data(),
        numModeInfoArrayElements, modeInfoArray.data(),
        SDC_APPLY | SDC_USE_SUPPLIED_DISPLAY_CONFIG | SDC_ALLOW_CHANGES | SDC_SAVE_TO_DATABASE
    );

    if (result == ERROR_SUCCESS) {
        printf("Settings applied successfully.\n");
    }
    else {
        printf("SetDisplayConfig failed: %d\n", result);
    }
}

#pragma endregion

#pragma region Monitor

Monitor::Monitor(const MonitorConfig& config) :
    m_device(nullptr),
    m_context(nullptr),
    m_Config(config),
    m_pVideoBuffer(nullptr),
    m_running(false),
    m_threadFinished(false)
{

}

Monitor::~Monitor() {
    m_running = false;

    while (!m_threadFinished) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // not a hard busy wait
    }

    if (m_pVideoBuffer) {
        delete m_pVideoBuffer;
    }
}

void Monitor::setID3D11Device(ID3D11Device* device) {
    m_device = device;
}

void Monitor::setID3D11DeviceContext(ID3D11DeviceContext* context) {
    m_context = context;
}

void Monitor::setFrameCallback(SendFrameCallback sendFrameCallback) {
    m_sendFrameCallback = sendFrameCallback;
}

bool Monitor::Initialize(const wchar_t* frameReadyName,
    const wchar_t* frameProcessedName,
    const wchar_t* sharedMemoryName,
    const wchar_t* sharedTextureName1,
    const wchar_t* sharedTextureName2)
{
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = m_Config.width;
    stagingDesc.Height = m_Config.height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING; // Special memory type for CPU readback
    stagingDesc.BindFlags = 0; // Staging cannot be a render target
    stagingDesc.MiscFlags = 0; // Staging cannot be shared between processes
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; // Allow CPU read

    HRESULT hr = m_device->CreateTexture2D(&stagingDesc, nullptr, &m_stagingTexture1);
    if (FAILED(hr)) {
        printf("Failed to create staging texture: 0x%08X\n", hr);
    }

    hr = m_device->CreateTexture2D(&stagingDesc, nullptr, &m_stagingTexture2);
    if (FAILED(hr)) {
        printf("Failed to create staging texture: 0x%08X\n", hr);
    }

    //m_gDisplay = new GpuDisplay(m_Config.width, m_Config.height, m_device.Get(), m_context.Get());

    m_pVideoBuffer = new VideoBuffer(m_Config.width, m_Config.height, m_Config.byteDepth);
    if (!m_pVideoBuffer->Initialize(m_device, frameReadyName, frameProcessedName, sharedMemoryName, sharedTextureName1, sharedTextureName2)) {
        printf("m_pVideoBuffer->Initialize failed.\n");
        return false;
    }

    m_runThread = std::thread(&Monitor::Run, this);
    m_runThread.detach();

    return true;
}

void Monitor::Run() {
    m_running = true;
    while (m_running)
    {
        DWORD waitResult = WaitForSingleObject(
            m_pVideoBuffer->GetFrameReadyEvent(),
            50
        );

        if (!m_running) {
            break;
        }

        switch (waitResult)
        {
        case WAIT_OBJECT_0:
        {
            auto frame = m_pVideoBuffer->GetLatestFrame();

            TimeProfiler::instance().stamp("createFrame");

            uint16_t staleIdx = frame.bufferIdx ^ 1;
            IDXGIKeyedMutex* staleMutex = (staleIdx == 0) ? m_pVideoBuffer->m_mutex1.Get() : m_pVideoBuffer->m_mutex2.Get();

            if (staleMutex->AcquireSync(1, 0) == S_OK) {
                staleMutex->ReleaseSync(0);
            }

            IDXGIKeyedMutex* currentMutex = (frame.bufferIdx == 0) ? m_pVideoBuffer->m_mutex1.Get() : m_pVideoBuffer->m_mutex2.Get();
            ID3D11Texture2D* stageTexture = (frame.bufferIdx == 0) ? m_stagingTexture1.Get() : m_stagingTexture2.Get();

            HRESULT hr = currentMutex->AcquireSync(1, 16);
            if (hr == WAIT_TIMEOUT) {
                printf("Timeout.\n");
                break;
            }
            else if (FAILED(hr)) {
                printf("[GpuDisplay] AcquireSync failed: 0x%08X\n", hr);
                break;
            }

            m_context->CopyResource(stageTexture, frame.texture);
            // 4. Release the mutex as soon as possible (Key 0 - return control to the driver)
            hr = currentMutex->ReleaseSync(0);

            // 2. Map staging texture
            D3D11_MAPPED_SUBRESOURCE mappedResource;

            hr = m_context->Map(stageTexture, 0, D3D11_MAP_READ, 0, &mappedResource);


            if (SUCCEEDED(hr))
            {

                UINT rowPitch = mappedResource.RowPitch;

                m_sendFrameCallback(shared_from_this(), frame.frameId, frame.size, rowPitch, mappedResource.pData);

                m_context->Unmap(stageTexture, 0);
            }
            else {
                printf("Failed to map texture. Error: 0x%lx\n", hr);
            }



            break;
        }

        case WAIT_TIMEOUT:
        {

            //printf("Frame wait timeout.\n");
            break;
        }

        case WAIT_FAILED:
            std::cerr << "WaitForSingleObject failed: " << GetLastError() << "\n";
            m_running = false;
            break;

        default:
            // Should not happen
            break;
        }
    }
    m_threadFinished = true;
}

std::thread& Monitor::GetThread() {
    return m_runThread;
}

#pragma endregion