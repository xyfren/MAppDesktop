#include "MonitorManager.h"


#pragma region MonitorManager

MonitorManager::MonitorManager() {
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
    return true;
}

bool MonitorManager::ConnectToDriver() {
    // Открываем устройство драйвера
    return WaitOpenDriver(200, 10000);
}

bool  MonitorManager::AddMonitor(uint16_t monitorId, uint16_t width, uint16_t height, uint16_t byteDepth, uint16_t refreshRate) {
    MonitorConfig config = {};
    config.monitorId = monitorId;
    config.width = width;
    config.height = height;
    config.byteDepth = byteDepth;
    config.refreshRate = refreshRate;
    config.enabled = true;

    Monitor* newMonitor = new Monitor(config,m_device.Get(),m_context.Get());

    m_Monitors.push_back(newMonitor);

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

    if (!newMonitor->Initialize(frameReadyName, frameProcessedName, sharedMemoryName, sharedTextureName1, sharedTextureName2)) {
        std::cout << "Ошибка инициализации\n";
        return false;
    }
    std::cout << "инициализация прошла успешно\n";

    CreateMonitorRequest request = {};
    request.config = config;
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
    delete m_Monitors[monitorId];
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

DriverInfo MonitorManager::GetDriverInfo() {
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

Monitor::Monitor(const MonitorConfig& config, ID3D11Device* device, ID3D11DeviceContext* context) :
    m_device(device),
    m_context(context),
    m_Config(config),
    m_pBuffer(nullptr),
    m_pVideoBuffer(nullptr),
    m_gDisplay(nullptr),
    m_running(false),
    m_threadFinished(false)
{}

Monitor::~Monitor() {
    m_running = false;
    // если поток может ждать событие, разбудите его (например, через событие остановки)

    while (!m_threadFinished) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // не жёсткий busy wait
    }

    if (m_pBuffer) {
        m_pBuffer->Cleanup();
    }
}

bool Monitor::Initialize(const wchar_t* frameReadyName, 
                         const wchar_t* frameProcessedName, 
                         const wchar_t* sharedMemoryName,
                         const wchar_t* sharedTextureName1, 
                         const wchar_t* sharedTextureName2)
{

    m_gDisplay = new GpuDisplay(m_Config.width, m_Config.height, m_device.Get(), m_context.Get());

    //if (!CreateSharedBuffer(frameReadyName, frameProcessedName, sharedMemoryName)) {
    //    return false;
    //}

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
    if (m_gDisplay->Initialize()) {
        m_running = true;
    }
    else {
        std::cout << "Ошибка иницализации GpuDisplay" << std::endl;
    }
    //m_running = true;
    while (m_running)
    {
        DWORD waitResult = WaitForSingleObject(
            m_pVideoBuffer->GetFrameReadyEvent(),       // handle события
            50      // ждать бесконечно (или 5000 мс, например)
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
            m_gDisplay->ShowFrame(frame.texture);
            std::cout << "Новый кадр " << "id = " << frame.frameId <<"; idx = " << frame.bufferIdx << std::endl;
   
            m_pVideoBuffer->MarkFrameProcessed();

            if (!m_gDisplay->ProcessEvents())
                m_running = false;

            break;
        }

        case WAIT_TIMEOUT:
        {
            if (!m_gDisplay->ProcessEvents())
                m_running = false;

            //std::cout << "Таймаут ожидания кадра\n";
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
    delete m_gDisplay;
    m_threadFinished = true;
}

std::thread& Monitor::GetThread() {
    return m_runThread;
}

bool Monitor::CreateSharedBuffer(const wchar_t* frameReadyName, const wchar_t* frameProcessedName, const wchar_t* sharedMemoryName)

{
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);  // NULL DACL = полный доступ всем

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    HANDLE hFrameReadyEvent = CreateEventW(
        &sa,               // lpSecurityAttributes (по умолчанию)
        TRUE,               // bManualReset = TRUE → manual reset (нужно ResetEvent)
        FALSE,              // bInitialState = FALSE → изначально несигнализировано
        frameReadyName   // имя (можно без имени — NULL, тогда анонимное)
    );

    HANDLE hFrameProcessedEvent = CreateEventW(
        &sa,
        TRUE,
        FALSE,
        frameProcessedName
    );

    if (!hFrameReadyEvent || !hFrameProcessedEvent)
    {
        std::cerr << "Не удалось создать события\n";
        return false;
    }
    std::cout << "События созданы успешно\n";

    HANDLE hSharedMemory = CreateFileMappingW(
        INVALID_HANDLE_VALUE,       // используем paging file
        &sa,                       // атрибуты по умолчанию
        PAGE_READWRITE,             // чтение + запись
        0,                          // старшие 32 бита размера
        m_Config.width * m_Config.height * m_Config.byteDepth * 2 + sizeof(DoubleBuffer::FrameHeader),
        sharedMemoryName
    );

    if (!hSharedMemory) {
        return false;
    }

    m_pBuffer = new DoubleBuffer();

    m_pBuffer->Initialize(hSharedMemory, hFrameReadyEvent, hFrameProcessedEvent, m_Config.width, m_Config.height, m_Config.byteDepth);

    return true;
}

#pragma endregion

