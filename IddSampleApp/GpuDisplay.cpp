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
    scd.BufferDesc.RefreshRate.Numerator = refreshRate;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // лучше для производительности
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

bool GpuDisplay::ShowFrame(ID3D11Texture2D* frameTexture, IDXGIKeyedMutex* mutex) {
    if (!frameTexture || !m_context || !m_swapChain || !mutex) {
        printf("[GpuDisplay] Invalid parameters: tex=%p, ctx=%p, sc=%p, mtx=%p\n",
            frameTexture, m_context.Get(), m_swapChain.Get(), mutex);
        return false;
    }

    // 1. Попытка захвата мьютекса
    HRESULT hr = mutex->AcquireSync(1, 16);
    if (hr == WAIT_TIMEOUT) {
        // Это не всегда ошибка, возможно драйвер просто не успел подготовить кадр
        return false;
    }
    else if (FAILED(hr)) {
        printf("[GpuDisplay] AcquireSync failed: 0x%08X\n", hr);
        return false;
    }

    // 2. Инициализация промежуточной текстуры (если еще не создана)
    //if (!m_internalCopy) {
    //    D3D11_TEXTURE2D_DESC desc = {};
    //    frameTexture->GetDesc(&desc);

    //    // Убираем флаги мьютекса для внутренней копии
    //    desc.MiscFlags = 0;
    //    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    //    desc.Usage = D3D11_USAGE_DEFAULT;

    //    hr = m_device->CreateTexture2D(&desc, nullptr, &m_internalCopy);
    //    if (FAILED(hr)) {
    //        printf("[GpuDisplay] CreateTexture2D (internal) failed: 0x%08X\n", hr);
    //        mutex->ReleaseSync(0); // Обязательно отпускаем, иначе драйвер зависнет
    //        return false;
    //    }
    //    printf("[GpuDisplay] Internal copy texture created successfully\n");
    //}

    // 3. Копирование из Shared текстуры во внутреннюю
    // Используем CopySubresourceRegion, так как у текстур разные MiscFlags
    //m_context->CopySubresourceRegion(m_internalCopy.Get(), 0, 0, 0, 0, frameTexture, 0, nullptr);
    m_context->CopyResource(m_backBuffer.Get(),frameTexture); // test
    // 4. Освобождаем мьютекс как можно быстрее (Ключ 0 - отдаем драйверу)
    hr = mutex->ReleaseSync(0);
    if (FAILED(hr)) {
        printf("[GpuDisplay] ReleaseSync failed: 0x%08X\n", hr);
        // Продолжаем, так как данные уже скопированы во внутреннюю текстуру
    }

    // 5. Получаем BackBuffer из SwapChain
    m_backBuffer.Reset();
    hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&m_backBuffer));
    if (FAILED(hr)) {
        printf("[GpuDisplay] GetBuffer failed: 0x%08X\n", hr);
        return false;
    }

    // 6. Копируем во фронтальный буфер и выводим
    //m_context->CopyResource(m_backBuffer.Get(), m_internalCopy.Get());

    // Present вызываем ОДИН раз
    hr = m_swapChain->Present(1, 0);
    if (FAILED(hr)) {
        printf("[GpuDisplay] Present failed: 0x%08X\n", hr);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            printf("[GpuDisplay] Device lost! Need to re-initialize D3D11.\n");
        }
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