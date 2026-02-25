#include "GpuDisplay.h"

GpuDisplay::GpuDisplay(int width, int height, ID3D11Device* device, ID3D11DeviceContext* context)
    : m_width(width), m_height(height), m_window(nullptr), m_device(device), m_context(context)
{
}

GpuDisplay::~GpuDisplay()
{
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
}

bool GpuDisplay::Initialize()
{
    // Create SDL window
    m_window = SDL_CreateWindow("Video Display", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        m_width, m_height, SDL_WINDOW_SHOWN);
    if (!m_window) return false;

    // Get HWND
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(m_window, &wmInfo)) return false;
    HWND hwnd = wmInfo.info.win.window;

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = m_width;
    scd.BufferDesc.Height = m_height;
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<IDXGIFactory> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    hr = factory->CreateSwapChain(m_device.Get(), &scd, &m_swapChain);
    if (FAILED(hr)) return false;

    // Get back buffer
    hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&m_backBuffer));
    if (FAILED(hr)) return false;

    return true;
}

bool GpuDisplay::ShowFrame(ID3D11Texture2D* frameTexture)
{
    if (!frameTexture || !m_context || !m_swapChain) {
        printf("FAIL\n");
        printf("0x%08X\n", m_context);
        printf("0x%08X\n", m_swapChain);
        printf("%d", frameTexture);
        return false;
    }
    HRESULT hr = m_context->GetData(nullptr, nullptr, 0, 0); // пустой запрос дл€ проверки
    if (FAILED(hr)) {
        OutputDebugStringA("GPU device removed or hung\n");
        return false;
    }

    m_context->CopyResource(m_backBuffer.Get(), frameTexture);

    // ѕопробуем без VSync дл€ теста
    hr = m_swapChain->Present(0, 0); // 0 = без вертикальной синхронизации

    if (FAILED(hr)) {
        char buf[100];
        sprintf_s(buf, "Present failed: 0x%08X\n", hr);
        OutputDebugStringA(buf);
        return false;
    }

    return true;
}

bool GpuDisplay::ProcessEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return false;
        }
        // Handle window resize if needed (would require recreating swap chain)
    }
    return true;
}