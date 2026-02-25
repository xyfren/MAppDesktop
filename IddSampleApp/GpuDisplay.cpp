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
    m_window = SDL_CreateWindow("Video Display", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        m_width, m_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!m_window) return false;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(m_window, &wmInfo)) return false;
    HWND hwnd = wmInfo.info.win.window;

    // 1. Запрашиваем текущую частоту монитора (реальную)
    int displayIndex = SDL_GetWindowDisplayIndex(m_window);
    SDL_DisplayMode mode;
    SDL_GetCurrentDisplayMode(displayIndex, &mode);
    int refreshRate = mode.refresh_rate; // например 120

    if (refreshRate <= 0) refreshRate = 120; // запасной вариант

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;                   // двойная буферизация
    scd.BufferDesc.Width = m_width;
    scd.BufferDesc.Height = m_height;
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 120;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    //scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // лучше для производительности
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

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

bool GpuDisplay::ShowFrame(ID3D11Texture2D* frameTexture) {
    if (!frameTexture || !m_context || !m_swapChain) {
        printf("Invalid parameters\n");
        return false;
    }

    m_context->CopyResource(m_backBuffer.Get(), frameTexture);
    //m_context->Flush();
    HRESULT hr = m_swapChain->Present(0, 0);
    if (FAILED(hr)) {
        printf("Present failed: 0x%08X\n", hr);
        return false;
    }

    return true;
}

bool GpuDisplay::ProcessEvents()
{
    // Handle window events
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return false;
        }
    }
    return true;
}