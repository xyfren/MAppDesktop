/*++

Copyright (c) Microsoft Corporation

Abstract:

    This module contains a sample implementation of an indirect display driver. See the included README.md file and the
    various TODO blocks throughout this file and all accompanying files for information on building a production driver.

    MSDN documentation on indirect displays can be found at https://msdn.microsoft.com/en-us/library/windows/hardware/mt761968(v=vs.85).aspx.

Environment:

    User Mode, UMDF

--*/

#include "Driver.h"
// В начало файла, после #include
#include <stdio.h>

// Простая функция для записи в лог-файл
void WriteDriverLog(const char* format, ...)
{
    FILE* f = nullptr;
    errno_t err = fopen_s(&f, "C:\\Windows\\Temp\\idd_driver.log", "a");

    if (err == 0 && f != nullptr)
    {
        // Получаем время
        time_t rawTime;
        time(&rawTime);
        struct tm timeInfo;
        localtime_s(&timeInfo, &rawTime);

        // Записываем временную метку
        fprintf(f, "[%02d:%02d:%02d] ",
            timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

        // Записываем сообщение
        va_list args;
        va_start(args, format);
        vfprintf(f, format, args);
        va_end(args);

        fprintf(f, "\n");
        fclose(f);
    }
}

void WriteDriverLogW(const wchar_t* format, ...)
{
    FILE* f = nullptr;
    errno_t err = fopen_s(&f, "C:\\Windows\\Temp\\idd_driver.log", "a");

    if (err == 0 && f != nullptr)
    {
        // Получаем время
        time_t rawTime;
        time(&rawTime);
        struct tm timeInfo;
        localtime_s(&timeInfo, &rawTime);

        // Записываем временную метку
        fprintf(f, "[%02d:%02d:%02d] ",
            timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

        // Конвертируем wchar_t в UTF-8 для записи в файл
        va_list args;
        va_start(args, format);

        // Создаем буфер для широкой строки
        wchar_t wideBuffer[1024];
        vswprintf_s(wideBuffer, 1024, format, args);

        // Конвертируем в UTF-8
        char utf8Buffer[2048];
        WideCharToMultiByte(CP_UTF8, 0, wideBuffer, -1, utf8Buffer, 2048, NULL, NULL);

        fprintf(f, "%s", utf8Buffer);
        va_end(args);

        fprintf(f, "\n");
        fclose(f);
    }
}

// Используй так:
#define DRV_LOG(...) WriteDriverLog(__VA_ARGS__)
#define DRV_LOGW(...) WriteDriverLogW(__VA_ARGS__)
//#include "Driver.tmh"

using namespace std;
using namespace Microsoft::IndirectDisp;
using namespace Microsoft::WRL;

#pragma region SampleMonitors

static constexpr DWORD IDD_SAMPLE_MONITOR_COUNT = 1; // If monitor count > ARRAYSIZE(s_SampleMonitors), we create edid-less monitors

// Default modes reported for edid-less monitors. The first mode is set as preferred
static const struct IndirectSampleMonitor::SampleMonitorMode s_SampleDefaultModes[] = 
{
    { 1920, 1080, 60 },
    { 1600,  900, 60 },
    { 1024,  768, 75 },

};

//static const struct IndirectSampleMonitor s_SampleMonitors[];

#pragma endregion

#pragma region helpers

static inline void FillSignalInfo(DISPLAYCONFIG_VIDEO_SIGNAL_INFO& Mode, DWORD Width, DWORD Height, DWORD VSync, bool bMonitorMode)
{
    Mode.totalSize.cx = Mode.activeSize.cx = Width;
    Mode.totalSize.cy = Mode.activeSize.cy = Height;

    // See https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_video_signal_info
    Mode.AdditionalSignalInfo.vSyncFreqDivider = bMonitorMode ? 0 : 1;
    Mode.AdditionalSignalInfo.videoStandard = 255;

    Mode.vSyncFreq.Numerator = VSync;
    Mode.vSyncFreq.Denominator = 1;
    Mode.hSyncFreq.Numerator = VSync * Height;
    Mode.hSyncFreq.Denominator = 1;

    Mode.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;

    Mode.pixelRate = ((UINT64) VSync) * ((UINT64) Width) * ((UINT64) Height);
}

static IDDCX_MONITOR_MODE CreateIddCxMonitorMode(DWORD Width, DWORD Height, DWORD VSync, IDDCX_MONITOR_MODE_ORIGIN Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER)
{
    IDDCX_MONITOR_MODE Mode = {};

    Mode.Size = sizeof(Mode);
    Mode.Origin = Origin;
    FillSignalInfo(Mode.MonitorVideoSignalInfo, Width, Height, VSync, true);

    return Mode;
}

static IDDCX_TARGET_MODE CreateIddCxTargetMode(DWORD Width, DWORD Height, DWORD VSync)
{
    IDDCX_TARGET_MODE Mode = {};

    Mode.Size = sizeof(Mode);
    FillSignalInfo(Mode.TargetVideoSignalInfo.targetVideoSignalInfo, Width, Height, VSync, false);

    return Mode;
}

#pragma endregion

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD IddSampleDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY IddSampleDeviceD0Entry;

EVT_IDD_CX_DEVICE_IO_CONTROL IddSampleDeviceIoControl;

EVT_IDD_CX_ADAPTER_INIT_FINISHED IddSampleAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES IddSampleAdapterCommitModes;

EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION IddSampleParseMonitorDescription;
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES IddSampleMonitorGetDefaultModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES IddSampleMonitorQueryModes;

EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN IddSampleMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN IddSampleMonitorUnassignSwapChain;

struct IndirectDeviceContextWrapper
{
    IndirectDeviceContext* pContext;

    void Cleanup()
    {
        delete pContext;
        pContext = nullptr;
    }
};

struct IndirectMonitorContextWrapper
{
    IndirectMonitorContext* pContext;

    void Cleanup()
    {
        delete pContext;
        pContext = nullptr;
    }
};

// This macro creates the methods for accessing an IndirectDeviceContextWrapper as a context for a WDF object
WDF_DECLARE_CONTEXT_TYPE(IndirectDeviceContextWrapper);

WDF_DECLARE_CONTEXT_TYPE(IndirectMonitorContextWrapper);

extern "C" BOOL WINAPI DllMain(
    _In_ HINSTANCE hInstance,
    _In_ UINT dwReason,
    _In_opt_ LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    UNREFERENCED_PARAMETER(dwReason);

    return TRUE;
}

_Use_decl_annotations_
extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT  pDriverObject,
    PUNICODE_STRING pRegistryPath
)
{
    DRV_LOG("DriverEntry");
    WDF_DRIVER_CONFIG Config;
    NTSTATUS Status;

    WDF_OBJECT_ATTRIBUTES Attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);

    WDF_DRIVER_CONFIG_INIT(&Config,
        IddSampleDeviceAdd
    );

    Status = WdfDriverCreate(pDriverObject, pRegistryPath, &Attributes, &Config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    return Status;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT pDeviceInit)
{
    NTSTATUS Status = STATUS_SUCCESS;
    WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);

    // Register for power callbacks - in this sample only power-on is needed
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
    PnpPowerCallbacks.EvtDeviceD0Entry = IddSampleDeviceD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(pDeviceInit, &PnpPowerCallbacks);

    IDD_CX_CLIENT_CONFIG IddConfig;
    IDD_CX_CLIENT_CONFIG_INIT(&IddConfig);

    // If the driver wishes to handle custom IoDeviceControl requests, it's necessary to use this callback since IddCx
    // redirects IoDeviceControl requests to an internal queue. This sample does not need this.
    IddConfig.EvtIddCxDeviceIoControl = IddSampleDeviceIoControl;

    IddConfig.EvtIddCxAdapterInitFinished = IddSampleAdapterInitFinished;

    IddConfig.EvtIddCxParseMonitorDescription = IddSampleParseMonitorDescription;
    IddConfig.EvtIddCxMonitorGetDefaultDescriptionModes = IddSampleMonitorGetDefaultModes;
    IddConfig.EvtIddCxMonitorQueryTargetModes = IddSampleMonitorQueryModes;
    IddConfig.EvtIddCxAdapterCommitModes = IddSampleAdapterCommitModes;
    IddConfig.EvtIddCxMonitorAssignSwapChain = IddSampleMonitorAssignSwapChain;
    IddConfig.EvtIddCxMonitorUnassignSwapChain = IddSampleMonitorUnassignSwapChain;

    Status = IddCxDeviceInitConfig(pDeviceInit, &IddConfig);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);
    Attr.EvtCleanupCallback = [](WDFOBJECT Object)
    {
        // Automatically cleanup the context when the WDF object is about to be deleted
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Object);
        if (pContext)
        {
            pContext->Cleanup();
        }
    };

    UNICODE_STRING  symLinkName = { 0 };

    RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\Global\\IddSampleDriver");

    WDFDEVICE Device = nullptr;
    Status = WdfDeviceCreate(&pDeviceInit, &Attr, &Device);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = WdfDeviceCreateSymbolicLink(Device,&symLinkName);
    DbgPrint("!!! Symbolic link creation );");
    // Утрированный пример того, как можно проверить результат выполнения операции
    if (!NT_SUCCESS(Status)) {
        DbgPrint("!!! Symbolic link creation failed: 0x%08X\n", Status);
        return STATUS_ERROR_PROCESS_NOT_IN_JOB;
    }

    Status = IddCxDeviceInitialize(Device);

    // Create a new device context object and attach it to the WDF device object
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext = new IndirectDeviceContext(Device);

    return Status;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    UNREFERENCED_PARAMETER(PreviousState);

    // This function is called by WDF to start the device in the fully-on power state.

    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pContext->pContext->InitAdapter();

    return STATUS_SUCCESS;
}

#pragma region Direct3DDevice

Direct3DDevice::Direct3DDevice(LUID AdapterLuid) : AdapterLuid(AdapterLuid)
{

}

Direct3DDevice::Direct3DDevice()
{
    AdapterLuid = LUID{};
}

HRESULT Direct3DDevice::Init()
{
    // The DXGI factory could be cached, but if a new render adapter appears on the system, a new factory needs to be
    // created. If caching is desired, check DxgiFactory->IsCurrent() each time and recreate the factory if !IsCurrent.
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory));
    if (FAILED(hr))
    {
        return hr;
    }

    // Find the specified render adapter
    hr = DxgiFactory->EnumAdapterByLuid(AdapterLuid, IID_PPV_ARGS(&Adapter));
    if (FAILED(hr))
    {
        return hr;
    }

    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    hr = D3D11CreateDevice(Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &DeviceContext);
    if (FAILED(hr))
    {
        // If creating the D3D device failed, it's possible the render GPU was lost (e.g. detachable GPU) or else the
        // system is in a transient state.
        return hr;
    }

    return S_OK;
}

#pragma endregion

#pragma region SwapChainProcessor

SwapChainProcessor::SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, shared_ptr<Direct3DDevice> Device, HANDLE NewFrameEvent, VideoBuffer* pVideoBuffer)
    : m_hSwapChain(hSwapChain), m_Device(Device), m_hAvailableBufferEvent(NewFrameEvent), m_pVideoBuffer(pVideoBuffer)
{
    m_hTerminateEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));
    m_frameCounter = 0;
    // Immediately create and run the swap-chain processing thread, passing 'this' as the thread parameter
    m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

SwapChainProcessor::~SwapChainProcessor()
{
    // Alert the swap-chain processing thread to terminate
    SetEvent(m_hTerminateEvent.Get());

    if (m_hThread.Get())
    {
        // Wait for the thread to terminate
        WaitForSingleObject(m_hThread.Get(), INFINITE);
    }
    if (m_pVideoBuffer)
        delete m_pVideoBuffer;
}

Direct3DDevice* SwapChainProcessor::GetD3DDevice() {
    return m_Device.get();
}

DWORD CALLBACK SwapChainProcessor::RunThread(LPVOID Argument)
{
    reinterpret_cast<SwapChainProcessor*>(Argument)->Run();
    return 0;
}

void SwapChainProcessor::Run()
{
    // For improved performance, make use of the Multimedia Class Scheduler Service, which will intelligently
    // prioritize this thread for improved throughput in high CPU-load scenarios.
    DWORD AvTask = 0;
    HANDLE AvTaskHandle = AvSetMmThreadCharacteristicsW(L"Distribution", &AvTask);

    RunCore();

    // Always delete the swap-chain object when swap-chain processing loop terminates in order to kick the system to
    // provide a new swap-chain if necessary.
    WdfObjectDelete((WDFOBJECT)m_hSwapChain);
    m_hSwapChain = nullptr;

    AvRevertMmThreadCharacteristics(AvTaskHandle);
}

void SwapChainProcessor::RunCore()
{
    // Get the DXGI device interface
    ComPtr<IDXGIDevice> DxgiDevice;
    HRESULT hr = m_Device->Device.As(&DxgiDevice);
    if (FAILED(hr))
    {
        return;
    }

    IDARG_IN_SWAPCHAINSETDEVICE SetDevice = {};
    SetDevice.pDevice = DxgiDevice.Get();

    hr = IddCxSwapChainSetDevice(m_hSwapChain, &SetDevice);
    if (FAILED(hr))
    {
        return;
    }

    // Acquire and release buffers in a loop
    DRV_LOG("LoopStart");
    for (;;)
    {
        ComPtr<IDXGIResource> AcquiredBuffer;

        // Ask for the next buffer from the producer
        IDARG_OUT_RELEASEANDACQUIREBUFFER Buffer = {};
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_hSwapChain, &Buffer);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if (hr == E_PENDING)
        {
            // We must wait for a new buffer
            HANDLE WaitHandles[] =
            {
                m_hAvailableBufferEvent,
                m_hTerminateEvent.Get()
            };
            DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(WaitHandles), WaitHandles, FALSE, 16);
            if (WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT)
            {
                // We have a new buffer, so try the AcquireBuffer again
                continue;
            }
            else if (WaitResult == WAIT_OBJECT_0 + 1)
            {
                // We need to terminate
                break;
            }
            else
            {
                // The wait was cancelled or something unexpected happened
                hr = HRESULT_FROM_WIN32(WaitResult);
                break;
            }
        }
        else if (SUCCEEDED(hr))
        {
            // We have new frame to process, the surface has a reference on it that the driver has to release
            AcquiredBuffer.Attach(Buffer.MetaData.pSurface);

            ComPtr<ID3D11Texture2D> texture;
            HRESULT hrTexture = AcquiredBuffer.As(&texture);
            
            if (SUCCEEDED(hrTexture) && m_pVideoBuffer)
            {
                
                // Отправляем текстуру в VideoBuffer для копирования и синхронизации
                // frameId можно увеличивать с каждым кадром, timestamp - например, QueryPerformanceCounter или __rdtsc
                m_pVideoBuffer->PushFrame(
                    texture.Get(),
                    m_Device->DeviceContext.Get(),
                    m_frameCounter++,           // frameId
                    __rdtsc()                    // timestamp
                );
            }

          
            AcquiredBuffer.Reset();

            // Indicate to OS that we have finished inital processing of the frame, it is a hint that
            // OS could start preparing another frame
            hr = IddCxSwapChainFinishedProcessingFrame(m_hSwapChain);
            if (FAILED(hr))
            {
                break;
            }

            // ==============================
            // TODO: Report frame statistics once the asynchronous encode/send work is completed
            //
            // Drivers should report information about sub-frame timings, like encode time, send time, etc.
            // ==============================
            // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
        }
        else
        {
            // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so exit the processing loop
            break;
        }
    }
}

#pragma endregion

#pragma region IndirectDeviceContext

IndirectDeviceContext::IndirectDeviceContext(_In_ WDFDEVICE WdfDevice) :
    m_WdfDevice(WdfDevice)
{
    m_Adapter = {};
    WdfWaitLockCreate(
        WDF_NO_OBJECT_ATTRIBUTES,  // Атрибуты по умолчанию
        &m_MonitorLock              // Указатель на создаваемый lock
    );
}

IndirectDeviceContext::~IndirectDeviceContext()
{
    for (int i = 0;i < m_Monitors.size();++i) {
        if (m_Monitors[i])
            delete m_Monitors[i];
    }
    m_Monitors.clear();
}

void IndirectDeviceContext::InitAdapter()
{
    // ==============================
    // TODO: Update the below diagnostic information in accordance with the target hardware. The strings and version
    // numbers are used for telemetry and may be displayed to the user in some situations.
    //
    // This is also where static per-adapter capabilities are determined.
    // ==============================

    IDDCX_ADAPTER_CAPS AdapterCaps = {};
    AdapterCaps.Size = sizeof(AdapterCaps);

    // Declare basic feature support for the adapter (required)
    AdapterCaps.MaxMonitorsSupported = IDD_SAMPLE_MONITOR_COUNT;
    AdapterCaps.EndPointDiagnostics.Size = sizeof(AdapterCaps.EndPointDiagnostics);
    AdapterCaps.EndPointDiagnostics.GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE;
    AdapterCaps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

    // Declare your device strings for telemetry (required)
    AdapterCaps.EndPointDiagnostics.pEndPointFriendlyName = L"IddSample Device";
    AdapterCaps.EndPointDiagnostics.pEndPointManufacturerName = L"Microsoft";
    AdapterCaps.EndPointDiagnostics.pEndPointModelName = L"IddSample Model";

    // Declare your hardware and firmware versions (required)
    IDDCX_ENDPOINT_VERSION Version = {};
    Version.Size = sizeof(Version);
    Version.MajorVer = 1;
    AdapterCaps.EndPointDiagnostics.pFirmwareVersion = &Version;
    AdapterCaps.EndPointDiagnostics.pHardwareVersion = &Version;

    // Initialize a WDF context that can store a pointer to the device context object
    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

    IDARG_IN_ADAPTER_INIT AdapterInit = {};
    AdapterInit.WdfDevice = m_WdfDevice;
    AdapterInit.pCaps = &AdapterCaps;
    AdapterInit.ObjectAttributes = &Attr;

    // Start the initialization of the adapter, which will trigger the AdapterFinishInit callback later
    IDARG_OUT_ADAPTER_INIT AdapterInitOut;
    NTSTATUS Status = IddCxAdapterInitAsync(&AdapterInit, &AdapterInitOut);

    if (NT_SUCCESS(Status))
    {
        // Store a reference to the WDF adapter handle
        m_Adapter = AdapterInitOut.AdapterObject;

        // Store the device context object into the WDF object context
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(AdapterInitOut.AdapterObject);
        pContext->pContext = this;
    }
}

// Ioctl handling

NTSTATUS IndirectDeviceContext::HandleIoctl(_In_ WDFREQUEST Request, _In_ size_t OutputLength,
    _In_ size_t InputLength, _In_ ULONG IoControlCode) 
{
    NTSTATUS status = STATUS_SUCCESS;

    switch (IoControlCode) {
    case IOCTL_IDD_ADD_MONITOR: {
        if (InputLength < sizeof(CreateMonitorRequest)) {
            return STATUS_INVALID_BUFFER_SIZE;
        }

        CreateMonitorRequest* pRequest = nullptr;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(CreateMonitorRequest),
            (PVOID*)&pRequest, nullptr);

        if (NT_SUCCESS(status)) {
            
            status = AddMonitor(pRequest);
            DRV_LOGW(L"Error %d",status);
        }
        break;
    }

    case IOCTL_IDD_REMOVE_MONITOR: {
        if (InputLength < sizeof(uint16_t)) {
            return STATUS_INVALID_BUFFER_SIZE;
        }

        uint16_t* pMonitorId = nullptr;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(uint16_t),
                                                (PVOID*)&pMonitorId, nullptr);
        if (NT_SUCCESS(status)) {
            status = RemoveMonitor(*pMonitorId);
        }
        break;
    }

    case IOCTL_IDD_GET_INFO: {
        if (OutputLength < sizeof(DriverInfo)) {
            return STATUS_INVALID_BUFFER_SIZE;
        }

        DriverInfo* pInfo = nullptr;
        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(DriverInfo),
                                                (PVOID*)&pInfo, nullptr);
        if (NT_SUCCESS(status)) {
            pInfo->monitorCount = (uint32_t)m_Monitors.size();
            pInfo->maxMonitors = 1;
            pInfo->driverVersion = 1;

            WdfRequestSetInformation(Request, sizeof(DriverInfo));
        }
        break;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;

    }
    
    WdfRequestComplete(Request, status);
    return status;

}

NTSTATUS IndirectDeviceContext::AddMonitor(CreateMonitorRequest* pRequest)
{
    MonitorConfig* pConfig = &pRequest->config;
    // 1. Проверяем лимит
    if (m_Monitors.size() >= 1) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (m_Monitors.size() < pConfig->monitorId) {
        return STATUS_INVALID_PARAMETER;
    }

    // 2. Создаём EDID для нового разрешения
    BYTE dynamicEdid[128];
    IddTools::GenerateEdid(dynamicEdid, pConfig->width, pConfig->height, pConfig->refreshRate);

    // 3. Создаём монитор
    IDDCX_MONITOR_INFO monitorInfo = {};
    monitorInfo.Size = sizeof(monitorInfo);
    monitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;
    monitorInfo.ConnectorIndex = pConfig->monitorId;

    monitorInfo.MonitorDescription.Size = sizeof(monitorInfo.MonitorDescription);
    monitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
    monitorInfo.MonitorDescription.DataSize = 128;
    monitorInfo.MonitorDescription.pData = dynamicEdid;

    CoCreateGuid(&monitorInfo.MonitorContainerId);

    // 4. WDF объект для монитора
    WDF_OBJECT_ATTRIBUTES attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, IndirectMonitorContextWrapper);

    IDARG_IN_MONITORCREATE createArgs = {};
    createArgs.ObjectAttributes = &attr;
    createArgs.pMonitorInfo = &monitorInfo;

    IDARG_OUT_MONITORCREATE createOut;
    NTSTATUS status = IddCxMonitorCreate(m_Adapter, &createArgs, &createOut);

    if (NT_SUCCESS(status)) {
        // 5. Сохраняем контекст 
        auto* pWrapper = WdfObjectGet_IndirectMonitorContextWrapper(createOut.MonitorObject);
        pWrapper->pContext = new IndirectMonitorContext(createOut.MonitorObject, pRequest);

        // 6. Добавляем в список
        m_Monitors.push_back(pWrapper->pContext); 

        // 7. Сообщаем системе
        IDARG_OUT_MONITORARRIVAL arrivalOut;
        status = IddCxMonitorArrival(createOut.MonitorObject, &arrivalOut);
    }

    return status;
}

NTSTATUS IndirectDeviceContext::RemoveMonitor(uint16_t MonitorId)
{
    // 1. Проверяем существование
    if (MonitorId >= m_Monitors.size()) {
        return STATUS_INVALID_PARAMETER;
    }

    // 2. Сообщаем системе, что монитор отключен
    //IDARG_OUT_MONITORARRIVAL departureOut;
    NTSTATUS status = IddCxMonitorDeparture(m_Monitors[MonitorId]->GetMonitorHandle());

    // 3. Удаляем из списка
    if (NT_SUCCESS(status)) {
        m_Monitors.erase(m_Monitors.begin() + MonitorId);

        // 4. Обновляем connectorIndex у оставшихся мониторов
        for (uint16_t i = MonitorId; i < m_Monitors.size(); i++) {
            // Нужно обновить внутренний ID монитора
        }
    }

    return status;
}

IndirectMonitorContext::IndirectMonitorContext(_In_ IDDCX_MONITOR Monitor, _In_ CreateMonitorRequest* pRequest)
{
    m_Monitor = Monitor;
    m_Config = pRequest->config;
    m_frameReadyName = pRequest->frameReadyName;
    m_frameProcessedName = pRequest->frameProcessedName;
    m_sharedMemoryName = pRequest->sharedMemoryName;
    m_sharedTextureName1 = pRequest->sharedTextureName1;
    m_sharedTextureName2 = pRequest->sharedTextureName2;
}

IndirectMonitorContext::~IndirectMonitorContext()
{
    m_ProcessingThread.reset();
}

IDDCX_MONITOR IndirectMonitorContext::GetMonitorHandle() const
{ 
    return m_Monitor; 
}

MonitorConfig IndirectMonitorContext::GetMonitorConfig() const 
{
    return m_Config;
}

void IndirectMonitorContext::AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent)
{
    m_ProcessingThread.reset();

    auto Device = make_shared<Direct3DDevice>(RenderAdapter);
    if (FAILED(Device->Init()))
    {   
        // It's important to delete the swap-chain if D3D initialization fails, so that the OS knows to generate a new
        // swap-chain and try again.
        WdfObjectDelete(SwapChain);
    }
    else
    {
        VideoBuffer* pVideoBuffer = new VideoBuffer(m_Config.width, m_Config.height, m_Config.byteDepth);
        if (!pVideoBuffer->Initialize(Device->Device,m_frameReadyName.c_str(),m_frameProcessedName.c_str(),m_sharedMemoryName.c_str(),m_sharedTextureName1.c_str(),m_sharedTextureName2.c_str())) {
            DRV_LOG("m_pVideoBuffer->Initialize error");
            return ;
        }
        m_ProcessingThread.reset(new SwapChainProcessor(SwapChain, Device, NewFrameEvent, pVideoBuffer));
    }
}

void IndirectMonitorContext::UnassignSwapChain()
{
    DRV_LOG("UnassignSwapChain");
    // Stop processing the last swap-chain
    m_ProcessingThread.reset();
}

#pragma endregion

#pragma region VideoBuffer

VideoBuffer::VideoBuffer(uint16_t width, uint16_t height, uint16_t byteDepth) {
    m_width = width;
    m_height = height;
    m_byteDepth = byteDepth;
}

VideoBuffer::~VideoBuffer() {
    DRV_LOG("Buffer deleted");
    Cleanup();
}

// This method has different realization
bool VideoBuffer::Initialize(
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    const wchar_t* frameReadyName,
    const wchar_t* frameProcessedName,
    const wchar_t* sharedInfoName,
    const wchar_t* sharedTextureName1,
    const wchar_t* sharedTextureName2)
{
    DWORD memErr;

    DRV_LOGW(sharedTextureName1);
    DRV_LOGW(sharedTextureName2);


    m_hFrameReadyEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, frameReadyName);
    if (m_hFrameReadyEvent == NULL) {
        DRV_LOG("OpenEventW m_hFrameReadyEvent returned handle=0x%p",
            m_hFrameReadyEvent);
        return false;
    }

    m_hFrameProcessedEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, frameProcessedName);
    if (m_hFrameProcessedEvent == NULL) {
        DRV_LOG("OpenEventW m_hFrameProcessedEvent returned handle=0x%p",
            m_hFrameProcessedEvent);
        return false;
    }
    
    m_hSharedInfo = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, sharedInfoName);
    if (m_hSharedInfo == NULL) {
        memErr = GetLastError();
        DRV_LOG("OpenFileMappingW returned handle=0x%p, LastError=%d",
            m_hSharedInfo, memErr);
        return false;
    }

    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&m_device1));
    if (FAILED(hr)) {
        memErr = GetLastError();
        DRV_LOG("QueryInterface returned hr=0x%p, LastError=%d",
            hr, memErr);
    }

    hr = m_device1->OpenSharedResourceByName(sharedTextureName1,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        IID_PPV_ARGS(&m_texture1));
    if (FAILED(hr)) {
        memErr = GetLastError();
        DRV_LOG("OpenSharedResourceByName returned hr=0x%p, LastError=%d",
            hr, memErr);
    }
    DRV_LOG("0x%p", m_texture1.Get());
    
    hr = m_device1->OpenSharedResourceByName(sharedTextureName2,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        IID_PPV_ARGS(&m_texture2));
    if (FAILED(hr)) {
        memErr = GetLastError();
        DRV_LOG("OpenSharedResourceByName returned hr=0x%p, LastError=%d",
            hr, memErr);
    }
    DRV_LOG("0x%p", m_texture2.Get());

    m_pMappedBuffer = MapViewOfFile(m_hSharedInfo, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!m_pMappedBuffer) return false;

    m_device1->GetImmediateContext(&m_context);
    DRV_LOG("VideoBuffer initialized successfully");
    return true;
}

void VideoBuffer::PushFrame(ID3D11Texture2D* sourceTexture, ID3D11DeviceContext* pContext, uint64_t frameId, uint64_t timestamp) {
    FrameHeader* header = (FrameHeader*)m_pMappedBuffer;

    uint16_t writeBuffer = header->freshBufferIdx ^ 1;

    if (header->bufferProccesed[writeBuffer]) {
        header->bufferProccesed[writeBuffer] = false;
        //DRV_LOG("NO_WAIT");
    }
    else {
        DWORD waitResult = WaitForSingleObject(
            m_hFrameProcessedEvent,       
            1000);
        switch (waitResult)
        {
        case WAIT_OBJECT_0:
        {
            DRV_LOG("WAIT_SUCC");
            header->bufferProccesed[writeBuffer] = false;
            ResetEvent(m_hFrameProcessedEvent);
            break;
        }
        case WAIT_TIMEOUT:
        {
            header->bufferProccesed[writeBuffer] = true;
            DRV_LOG("WAIT_TIMEOUT");
            break;
        }
        default:
        {
            ResetEvent(m_hFrameProcessedEvent);
            break;
        }
        }
    }

    if (writeBuffer == 0) {
        m_context->CopyResource(m_texture1.Get(), sourceTexture);
    }
    else {
        m_context->CopyResource(m_texture2.Get(), sourceTexture);
    }

    // Ensure GPU copy is complete before signaling the app
    //m_context->Flush();

    header->frameId = frameId;
    header->timestamp = timestamp;

    // Memory barrier to ensure all writes to the shared header are visible
    // to the app process before it reads freshBufferIdx
    MemoryBarrier();
    header->freshBufferIdx = writeBuffer;

    SetEvent(m_hFrameReadyEvent);
}

void VideoBuffer::MarkFrameProcessed() {
    //Only for app
}

VideoBuffer::Frame VideoBuffer::GetLatestFrame() {
    //Only for app
    return Frame();
}

void VideoBuffer::Cleanup()
{
    if (m_pMappedBuffer) UnmapViewOfFile(m_pMappedBuffer);
    if (m_hSharedInfo) CloseHandle(m_hSharedInfo);
    if (m_hFrameReadyEvent) CloseHandle(m_hFrameReadyEvent);
    if (m_hFrameProcessedEvent) CloseHandle(m_hFrameProcessedEvent);

    m_pMappedBuffer = nullptr;
    m_hSharedInfo = nullptr;
    m_hFrameReadyEvent = nullptr;
    m_hFrameProcessedEvent = nullptr;
}

#pragma endregion


#pragma region DDI Callbacks

_Use_decl_annotations_
NTSTATUS IddSampleAdapterInitFinished(IDDCX_ADAPTER AdapterObject, const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs)
{
    // We can send signal to application to notify them
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleAdapterCommitModes(IDDCX_ADAPTER AdapterObject, const IDARG_IN_COMMITMODES* pInArgs)
{
    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);

    // For the sample, do nothing when modes are picked - the swap-chain is taken care of by IddCx

    // ==============================
    // TODO: In a real driver, this function would be used to reconfigure the device to commit the new modes. Loop
    // through pInArgs->pPaths and look for IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
    // should be turned off).
    // ==============================

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleParseMonitorDescription(const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs, IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs)
{
    UNREFERENCED_PARAMETER(pInArgs);
    UNREFERENCED_PARAMETER(pOutArgs);

    const DWORD totalModeCount = 1; // 1 кастом + базовые

    if (pInArgs->MonitorModeBufferInputCount == 0)
    {
        pOutArgs->MonitorModeBufferOutputCount = totalModeCount;
        return STATUS_SUCCESS;
    }

    if (pInArgs->MonitorModeBufferInputCount < totalModeCount)
    {
        pOutArgs->MonitorModeBufferOutputCount = totalModeCount;
        return STATUS_BUFFER_TOO_SMALL;
    }

    DWORD modeIndex = 0;

    // Try to parse EDID
    if (pInArgs->MonitorDescription.Type == IDDCX_MONITOR_DESCRIPTION_TYPE_EDID &&
        pInArgs->MonitorDescription.DataSize >= 128 &&
        pInArgs->MonitorDescription.pData != nullptr)
    {
        BYTE* edid = (BYTE*) pInArgs->MonitorDescription.pData;
        BYTE* dtd = edid + 54; // Первый DTD

        UINT16 pixelClock = dtd[0] | (dtd[1] << 8);

        if (pixelClock != 0)
        {
            UINT16 width = dtd[2] | ((dtd[4] & 0xF0) << 4); // Ширина (горизонтальные активные пиксели)
            UINT16 height = dtd[5] | ((dtd[7] & 0xF0) << 4); // Высота (вертикальные активные линии)
            
            UINT16 hBlanking = dtd[3] | ((dtd[4] & 0x0F) << 8); // Горизонтальное 
            UINT16 vBlanking = dtd[6] | ((dtd[7] & 0x0F) << 8); // Вертикальное гашение
            
            UINT16 hTotal = width + hBlanking;
            UINT16 vTotal = height + vBlanking;

            double refreshRate = (pixelClock * 10000.0) / (hTotal * vTotal); // Частота обновления в герцах
            DRV_LOG("SCREEN w=%d;h=%d;rate=%d",width,height,refreshRate);
            // Добавляем режим из EDID
            pInArgs->pMonitorModes[modeIndex++] = CreateIddCxMonitorMode(
                width,
                height,
                (UINT16)refreshRate,
                IDDCX_MONITOR_MODE_ORIGIN_MONITORDESCRIPTOR
            );
            
        }
    }

    pOutArgs->MonitorModeBufferOutputCount = modeIndex;
    pOutArgs->PreferredMonitorModeIdx = 0;

    return STATUS_SUCCESS;

}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorGetDefaultModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs, IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs)
{
    UNREFERENCED_PARAMETER(MonitorObject);

    // ==============================
    // In a real driver, this function would be called to generate monitor modes for a monitor with no EDID.
    // Drivers should report modes that are guaranteed to be supported by the transport protocol and by nearly all
    // monitors (such 640x480, 800x600, or 1024x768). If the driver has access to monitor modes from a descriptor other
    // than an EDID, those modes would also be reported here.
    // ==============================

    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
    if (!pMonitorContextWrapper || !pMonitorContextWrapper->pContext) {
        return STATUS_INVALID_PARAMETER;
    }

    IndirectMonitorContext* pContext = pMonitorContextWrapper->pContext;

    MonitorConfig monConfig = pContext->GetMonitorConfig();

    const DWORD totalModeCount = 1 + ARRAYSIZE(s_SampleDefaultModes);

    if (pInArgs->DefaultMonitorModeBufferInputCount == 0)
    {
        pOutArgs->DefaultMonitorModeBufferOutputCount = totalModeCount;
    }
    else
    {
        DWORD modeIndex = 0;

        pInArgs->pDefaultMonitorModes[modeIndex++] = CreateIddCxMonitorMode(
            monConfig.width,
            monConfig.height,
            monConfig.refreshRate,
            IDDCX_MONITOR_MODE_ORIGIN_DRIVER
        );

        for (DWORD i = 0; i < ARRAYSIZE(s_SampleDefaultModes); i++)
        {
            pInArgs->pDefaultMonitorModes[modeIndex++] = CreateIddCxMonitorMode(
                s_SampleDefaultModes[i].Width,
                s_SampleDefaultModes[i].Height,
                s_SampleDefaultModes[i].VSync,
                IDDCX_MONITOR_MODE_ORIGIN_DRIVER
            );
        }

        pOutArgs->DefaultMonitorModeBufferOutputCount = totalModeCount;
        pOutArgs->PreferredMonitorModeIdx = 0;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorQueryModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_QUERYTARGETMODES* pInArgs, IDARG_OUT_QUERYTARGETMODES* pOutArgs)
{
    UNREFERENCED_PARAMETER(MonitorObject);

    vector<IDDCX_TARGET_MODE> TargetModes;

    // Create a set of modes supported for frame processing and scan-out. These are typically not based on the
    // monitor's descriptor and instead are based on the static processing capability of the device. The OS will
    // report the available set of modes for a given output as the intersection of monitor modes with target modes.

    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);

    if (pMonitorContextWrapper && pMonitorContextWrapper->pContext)
    {
        MonitorConfig monConfig = pMonitorContextWrapper->pContext->GetMonitorConfig();
        TargetModes.push_back(CreateIddCxTargetMode(
            monConfig.width,
            monConfig.height,
            monConfig.refreshRate
        ));
    }

    //TargetModes.push_back(CreateIddCxTargetMode(3840, 2160, 60));
    //TargetModes.push_back(CreateIddCxTargetMode(2560, 1440, 144));
    //TargetModes.push_back(CreateIddCxTargetMode(2560, 1440, 90));
    //TargetModes.push_back(CreateIddCxTargetMode(2560, 1440, 60));
    //TargetModes.push_back(CreateIddCxTargetMode(1920, 1080, 144));
    //TargetModes.push_back(CreateIddCxTargetMode(1920, 1080, 90));
    //TargetModes.push_back(CreateIddCxTargetMode(1920, 1080, 60));
    //TargetModes.push_back(CreateIddCxTargetMode(1600,  900, 60));
    //TargetModes.push_back(CreateIddCxTargetMode(1280,  720, 60));
    //TargetModes.push_back(CreateIddCxTargetMode(1024,  768, 75));
    //TargetModes.push_back(CreateIddCxTargetMode(1024,  768, 60));

    pOutArgs->TargetModeBufferOutputCount = (UINT) TargetModes.size();

    if (pInArgs->TargetModeBufferInputCount >= TargetModes.size())
    {
        copy(TargetModes.begin(), TargetModes.end(), pInArgs->pTargetModes);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorAssignSwapChain(IDDCX_MONITOR MonitorObject, const IDARG_IN_SETSWAPCHAIN* pInArgs)
{
    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
    pMonitorContextWrapper->pContext->AssignSwapChain(pInArgs->hSwapChain, pInArgs->RenderAdapterLuid, pInArgs->hNextSurfaceAvailable);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject)
{
    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
    pMonitorContextWrapper->pContext->UnassignSwapChain();
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID IddSampleDeviceIoControl(
    WDFDEVICE Device,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode
)
{
    auto* pDeviceContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
    pDeviceContext->pContext->HandleIoctl(Request, OutputBufferLength, InputBufferLength, IoControlCode);
}

#pragma endregion
