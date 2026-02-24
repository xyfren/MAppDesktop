#include "../VideoBuffer.h"

VideoBuffer::VideoBuffer(uint16_t width, uint16_t height, uint16_t byteDepth) 
{
    m_width = width;
    m_height = height;
    m_byteDepth = m_byteDepth;
}

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
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {};

    desc.Width = m_width;
    desc.Height = m_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture1, texture2;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture1);
    hr = device->CreateTexture2D(&desc, nullptr, &texture2);

    // Получаем интерфейсы IDXGIResource1 для создания именованных shared-ресурсов
    Microsoft::WRL::ComPtr<IDXGIResource1> resource1, resource2;
    texture1.As(&resource1);
    texture2.As(&resource2);

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
    return true;
}

void VideoBuffer::VideoBufferPushFrame() {
    //Only for driver
}

void VideoBuffer::VideoBufferMarkFrameProcessed() {

}

void VideoBuffer::GetLatestFrame() {

}
