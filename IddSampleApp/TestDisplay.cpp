#include "TestDisplay.h"

TestDisplay::TestDisplay(int width, int height, ID3D11Device* device, ID3D11DeviceContext* context)
    : m_width(width), m_height(height), m_device(device), m_context(context) {

}

bool TestDisplay::Initialize() {
    // Создаём OpenCV окно
    cv::namedWindow("Display", cv::WINDOW_NORMAL);
    cv::resizeWindow("Display", m_width, m_height);

    return true;
}

bool TestDisplay::ShowFrame(ID3D11Texture2D* texture) {
    if (!texture || !m_device || !m_context) {
        printf("Invalid parameters\n");
        return false;
    }

    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    printf("Texture: %dx%d, format %d\n", desc.Width, desc.Height, desc.Format);

    // 1. Создаём staging текстуру
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = desc.Width;
    stagingDesc.Height = desc.Height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = desc.Format;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
    HRESULT hr = m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        printf("Failed to create staging texture\n");
        return false;
    }

    // 2. Копируем
    m_context->CopyResource(stagingTexture.Get(), texture);
    m_context->Flush();

    // 3. Маппим
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        printf("Failed to map staging texture\n");
        return false;
    }
    uint8_t* data = (uint8_t*)mapped.pData;
    uint8_t B = data[5000+0];
    uint8_t G = data[5000+1];
    uint8_t R = data[5000+2];
    uint8_t A = data[5000+3];

    std::cout << "Первый пиксель (BGRA): "
        << (int)B << " "
        << (int)G << " "
        << (int)R << " "
        << (int)A << std::endl;
    // 4. Создаём OpenCV изображение (копируем данные)
    cv::Mat frame(desc.Height, desc.Width, CV_8UC4);

    // Копируем с учётом RowPitch (построчно)
    for (UINT y = 0; y < desc.Height; y++) {
        memcpy(frame.ptr(y),
            (BYTE*)mapped.pData + y * mapped.RowPitch,
            desc.Width * 4);
    }

    // 5. Освобождаем ресурсы
    m_context->Unmap(stagingTexture.Get(), 0);

    // 6. Отображаем
    if (!frame.empty()) {
        cv::imshow("Display", frame);
        cv::waitKey(1);
        return true;
    }

    return false;
}

bool TestDisplay::ProcessEvents() {
    cv::imshow("Display", m_frame);
    // OpenCV тоже нужно дать возможность обработать события
    if (cv::getWindowProperty("Display", cv::WND_PROP_VISIBLE) < 1) {
        return false;
    }

    return true;
}