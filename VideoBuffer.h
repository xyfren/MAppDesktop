#pragma once

#include <dxgi.h>
#include <d3d11.h>
#include <dxgi1_5.h>
#include <d3d11_2.h>
#include <d3d11_1.h>
#include <avrt.h>
#include <wrl.h>
#include <time.h>
#include <windows.h>
#include <stdint.h>
#include <cstdio>

class VideoBuffer
{
public:
	struct Frame {
		const uint8_t* pixels;
		uint32_t size;
		uint64_t frameId;
		uint64_t timestamp;
		uint16_t width;
		uint16_t height;
		uint16_t byteDepth;
		uint16_t bufferIndex; // Какой буфер читаем
	};
	struct FrameHeader {
		uint64_t frameId;
		uint64_t timestamp;
		uint16_t width;
		uint16_t height;
		uint16_t byteDepth;
		uint32_t format;
		uint32_t frameSize;
		uint16_t bufferIndex; // Какой буфер содержит свежий кадр
		//uint16_t processedBufferIndex; // Какой буфер уже обработан приложением
		bool bufferProccesed[2];
	};

	bool Initialize(
		Microsoft::WRL::ComPtr<ID3D11Device> m_device,
		const wchar_t* m_frameReadyName,
		const wchar_t* m_frameProcessedName,
		const wchar_t* m_sharedTextureName1,
		const wchar_t* m_sharedTextureName2
	);

	void VideoBufferPushFrame();

	void VideoBufferMarkFrameProcessed();

	void GetLatestFrame();

private:
	HANDLE m_hFrameReadyEvent = nullptr; // Драйвер -> Приложение
	HANDLE m_hFrameProcessedEvent = nullptr; // Приложение -> Драйвер

	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture1, m_texture2;
	Microsoft::WRL::ComPtr<ID3D11Device1> m_device1;
};

