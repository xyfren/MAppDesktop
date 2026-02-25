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

#include "Common.h"

class VideoBuffer
{
public:
	struct Frame {
		ID3D11Texture2D* texture;
		uint32_t size;
		uint64_t frameId;
		uint64_t timestamp;
		uint16_t width;
		uint16_t height;
		uint16_t byteDepth;
		uint16_t bufferIdx; // Какой буфер читаем
	};
	struct FrameHeader {
		uint64_t frameId;
		uint64_t timestamp;
		uint16_t width;
		uint16_t height;
		uint16_t byteDepth;
		uint32_t format;
		uint32_t frameSize;
		uint16_t freshBufferIdx; // Какой буфер содержит свежий кадр (Заполнен драйвером)
		uint16_t processingBufferIdx; // Какой буфер сейчас обрабатывается приложением (Заполняется приложением)
		bool bufferProccesed[2];
	};

	VideoBuffer(uint16_t width, uint16_t height, uint16_t byteDepth);
	~VideoBuffer();

	bool Initialize(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		const wchar_t* frameReadyName,
		const wchar_t* frameProcessedName,
		const wchar_t* sharedInfoName,
		const wchar_t* sharedTextureName1,
		const wchar_t* sharedTextureName2
	);

	void PushFrame(ID3D11Texture2D* sourceTexture, ID3D11DeviceContext* pContext, uint64_t frameId, uint64_t timestamp);

	void MarkFrameProcessed();

	Frame GetLatestFrame();

	HANDLE GetFrameProcessedEvent() const;

	HANDLE GetFrameReadyEvent() const;

private:
	void Cleanup();

	uint16_t m_width = 0; 
	uint16_t m_height = 0; 
	uint16_t m_byteDepth = 0;

	HANDLE m_hSharedInfo = nullptr;
	HANDLE m_hFrameReadyEvent = nullptr; // Драйвер -> Приложение
	HANDLE m_hFrameProcessedEvent = nullptr; // Приложение -> Драйвер

	void* m_pMappedBuffer = nullptr;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture1, m_texture2;
	Microsoft::WRL::ComPtr<ID3D11Device1> m_device1;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
};

