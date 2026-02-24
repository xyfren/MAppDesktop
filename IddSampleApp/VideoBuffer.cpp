#include "../VideoBuffer.h"

bool VideoBuffer::Initialize(
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    const wchar_t* frameReadyName,
    const wchar_t* frameProcessedName,
    const wchar_t* sharedTextureName1,
    const wchar_t* sharedTextureName2) 
{
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&m_device1));
    DWORD memErr = GetLastError();
    printf("QueryInterface returned hr=0x%p, LastError=%d",
        hr, memErr);
  
    hr = m_device1->OpenSharedResourceByName(sharedTextureName1,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        IID_PPV_ARGS(&m_texture1));

    memErr = GetLastError();
    printf("OpenSharedResourceByName returned hr=0x%p, LastError=%d",
        hr, memErr);

    hr = m_device1->OpenSharedResourceByName(sharedTextureName2,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        IID_PPV_ARGS(&m_texture2));

    memErr = GetLastError();
    printf("OpenSharedResourceByName returned hr=0x%p, LastError=%d",
        hr, memErr);
    return true;
}

void VideoBuffer::VideoBufferPushFrame() {
    //Only for driver
}

void VideoBuffer::VideoBufferMarkFrameProcessed() {

}

void VideoBuffer::GetLatestFrame() {

}
