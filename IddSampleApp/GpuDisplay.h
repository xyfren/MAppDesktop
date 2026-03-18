#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <d3d11.h>
#include <wrl.h>

class GpuDisplay {
public:
    GpuDisplay(int width, int height, ID3D11Device* device, ID3D11DeviceContext* context);
    ~GpuDisplay();

    bool Initialize();
    bool ShowFrame(ID3D11Texture2D* frameTexture, IDXGIKeyedMutex* mutex);
    bool ProcessEvents(); // returns false if quit requested

private:
    int m_width, m_height;
    SDL_Window* m_window;
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_backBuffer;
    //Microsoft::WRL::ComPtr<ID3D11Texture2D> m_internalCopy;
};