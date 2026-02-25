#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/core/directx.hpp>
#include <d3d11.h>
#include <wrl.h>
#include <iostream>

class TestDisplay {
public:
    TestDisplay(int width, int height, ID3D11Device* device, ID3D11DeviceContext* context);

    bool Initialize();  // создаёт SDL окно
    bool ShowFrame(ID3D11Texture2D* texture);  // отображает текстуру через OpenCV
    bool ProcessEvents(); // возвращает false при закрытии

private:
    int m_width, m_height;
    cv::Mat m_frame;  // буфер для отображения
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
};