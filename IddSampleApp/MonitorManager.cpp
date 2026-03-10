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
        nullptr,                      // адаптер по умолчанию
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
        return false;
    }
    return true;
}

bool MonitorManager::ConnectToDriver() {
    return WaitOpenDriver(200, 10000);
}

bool MonitorManager::AddMonitor(std::shared_ptr<Monitor> pMonitor) {
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
        std::cout << "Ошибка инициализации\n";
        return false;
    }
    std::cout << "инициализация прошла успешно\n";

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

    printf("DeviceIoControl SUCCESS! Bytes returned: %lu\n", bytesReturned);
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

    printf("DeviceIoControl SUCCESS! Bytes returned: %lu\n", bytesReturned);
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

    printf("DeviceIoControl SUCCESS! Bytes returned: %lu\n", bytesReturned);
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
            break;
        }

        if (lastError != ERROR_FILE_NOT_FOUND &&
            lastError != ERROR_PATH_NOT_FOUND &&
            lastError != ERROR_DEVICE_NOT_CONNECTED &&
            lastError != ERROR_GEN_FAILURE)
        {
            break;
        }

        // Сколько осталось ждать
        DWORD remain = (DWORD)(maxTotalTimeMs - elapsed);
        DWORD sleepTime = (remain < intervalMs) ? remain : intervalMs;

        if (sleepTime == 0) break;

        Sleep(sleepTime);
    }
    return false;
}

#pragma endregion

#pragma region Monitor

Monitor::Monitor(const MonitorConfig& config):
    m_device(nullptr),
    m_context(nullptr),
    m_Config(config),
    m_pVideoBuffer(nullptr),
    m_gDisplay(nullptr),
    m_running(false),
    m_threadFinished(false)
{
   
}

Monitor::~Monitor() {
    m_running = false;

    while (!m_threadFinished) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // не жёсткий busy wait
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
    stagingDesc.Usage = D3D11_USAGE_STAGING; // Специальный тип памяти для передачи на CPU
    stagingDesc.BindFlags = 0; // Staging не может быть Render Target
    stagingDesc.MiscFlags = 0; // Staging нельзя расшарить между процессами
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; // Разрешаем чтение процессору

    HRESULT hr = m_device->CreateTexture2D(&stagingDesc, nullptr, &m_stagingTexture);
    if (FAILED(hr)) {
        printf("Failed to create staging texture: 0x%08X\n", hr);
    }

    //m_gDisplay = new GpuDisplay(m_Config.width, m_Config.height, m_device.Get(), m_context.Get());

    m_pVideoBuffer = new VideoBuffer(m_Config.width, m_Config.height, m_Config.byteDepth);
    if (!m_pVideoBuffer->Initialize(m_device,frameReadyName,frameProcessedName,sharedMemoryName,sharedTextureName1,sharedTextureName2)) {
        printf("m_pVideoBuffer->Initialize ERROR\n");
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
            ResetEvent(m_pVideoBuffer->GetFrameReadyEvent());
            auto frame = m_pVideoBuffer->GetLatestFrame();

            //std::cout << "Новый кадр " << "id = " << frame.frameId <<"; idx = " << frame.bufferIdx << std::endl;
            D3D11_TEXTURE2D_DESC desk;
            frame.texture->GetDesc(&desk);


            m_context->CopyResource(m_stagingTexture.Get(), frame.texture);

            // 2. Мапим Staging-текстуру
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            HRESULT hr = m_context->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
            
            if (SUCCEEDED(hr))
            {

                UINT rowPitch = mappedResource.RowPitch;

                m_sendFrameCallback(shared_from_this(), frame.frameId, frame.size, rowPitch, mappedResource.pData);

                m_context->Unmap(m_stagingTexture.Get(), 0);
            }
            else {
				printf("Failed to map texture. Error: 0x%lx\n", hr);
            }

            m_pVideoBuffer->MarkFrameProcessed();

            break;
        }

        case WAIT_TIMEOUT:
        {

            //std::cout << "Таймаут ожидания кадра" << std::endl;
            break;
        }

        case WAIT_FAILED:
            std::cerr << "Ошибка WaitForSingleObject: " << GetLastError() << "\n";
            m_running = false;
            break;

        default:
            // Не должно произойти
            break;
        }
    }
    m_threadFinished = true;
}

std::thread& Monitor::GetThread() {
    return m_runThread;
}

#pragma endregion

