#include "../VideoBuffer.h"

VideoBuffer::VideoBuffer(uint16_t width, uint16_t height, uint16_t byteDepth) 
{
    m_width = width;
    m_height = height;
    m_byteDepth = byteDepth;
}

VideoBuffer::~VideoBuffer() 
{
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
    if (!device) {
        return false;
    }
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&m_device1));
    DWORD memErr = GetLastError();
    if (FAILED(hr))
    {
        printf("QueryInterface returned hr=0x%p, LastError=%d",
            hr, memErr);
    }

    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);  // NULL DACL = полный доступ всем
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    m_hFrameReadyEvent = CreateEventW(&sa, FALSE, FALSE, frameReadyName);

    m_hFrameProcessedEvent = CreateEventW(&sa, FALSE, FALSE, frameProcessedName);

    if (!m_hFrameReadyEvent || !m_hFrameProcessedEvent)
    {
        printf("Не удалось создать события\n");
        return false;
    }
    printf("События созданы успешно\n");

    m_hSharedInfo = CreateFileMappingW(
        INVALID_HANDLE_VALUE,       // используем paging file
        &sa,                       // атрибуты по умолчанию
        PAGE_READWRITE,             // чтение + запись
        0,                          // старшие 32 бита размера
        sizeof(VideoBuffer::FrameHeader),
        sharedInfoName);

    if (!m_hSharedInfo) {
        printf("Не удалось создать общую память\n");
        return false;
    }

    m_pMappedBuffer = MapViewOfFile(m_hSharedInfo, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!m_pMappedBuffer) return false;

    FrameHeader* header = (FrameHeader*)m_pMappedBuffer;
    header->width = m_width;
    header->height = m_height;
    header->byteDepth = m_byteDepth;
    header->format = 87;
    header->frameSize = m_width * m_height * m_byteDepth;
    header->freshBufferIdx = 0;
    header->processingBufferIdx = 0;
    header->bufferProccesed[0] = true;
    header->bufferProccesed[1] = true;

    D3D11_TEXTURE2D_DESC desc = {};
       
    desc.Width = m_width;
    desc.Height = m_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

    printf("%d,%d", m_width, m_height);

    hr = device->CreateTexture2D(&desc, nullptr, &m_texture1);
    if (FAILED(hr)) {
        printf("CreateTexture2D failed for texture1, hr=0x%08X", hr);
        return false;
    }
    hr = device->CreateTexture2D(&desc, nullptr, &m_texture2);
    if (FAILED(hr)) {
        printf("CreateTexture2D failed for texture2, hr=0x%08X", hr);
        return false;
    }

    // Получаем интерфейсы IDXGIResource1 для создания именованных shared-ресурсов
    Microsoft::WRL::ComPtr<IDXGIResource1> resource1, resource2;
    m_texture1.As(&resource1);
    m_texture2.As(&resource2);

    HANDLE handle0 = nullptr, handle1 = nullptr;

    hr = resource1->CreateSharedHandle(&sa,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        sharedTextureName1, &handle0);
    if (FAILED(hr)) {
        printf("CreateSharedHandle failed for texture1, hr=0x%08X", hr);
        return false;
    }
    hr = resource2->CreateSharedHandle(&sa,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        sharedTextureName2, &handle1);
    if (FAILED(hr)) {
        printf("CreateSharedHandle failed for texture2, hr=0x%08X", hr);
        CloseHandle(handle0); // не забыть закрыть уже открытый handle
        return false;
    }

    m_device1->GetImmediateContext(&m_context);

    return true;
}

void VideoBuffer::PushFrame(ID3D11Texture2D* sourceTexture, ID3D11DeviceContext* pContext, uint64_t frameId, uint64_t timestamp) {
    //Only for driver
}

void VideoBuffer::MarkFrameProcessed() {
    FrameHeader* header = (FrameHeader*)m_pMappedBuffer;
    header->bufferProccesed[header->processingBufferIdx] = true;
    SetEvent(m_hFrameProcessedEvent);
}

VideoBuffer::Frame VideoBuffer::GetLatestFrame() 
{
    Frame frame = {};

    if (!m_pMappedBuffer) return frame;
    FrameHeader* header = (FrameHeader*)m_pMappedBuffer;
    

    header->processingBufferIdx = header->freshBufferIdx;
    //header->bufferProccesed[header->processingBufferIdx] = false;
    frame.texture = (header->processingBufferIdx == 0) ? m_texture1.Get() : m_texture2.Get();
    frame.width = header->width;
    frame.height = header->height;
    frame.byteDepth = header->byteDepth;
    frame.size = header->frameSize;
    frame.bufferIdx = header->processingBufferIdx;
    frame.frameId = header->frameId;
    frame.timestamp = header->timestamp;

    return frame;
}

HANDLE VideoBuffer::GetFrameProcessedEvent() const
{
    return m_hFrameProcessedEvent;
}

HANDLE VideoBuffer::GetFrameReadyEvent() const
{
    return m_hFrameReadyEvent;
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
