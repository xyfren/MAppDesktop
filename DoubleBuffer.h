// This file common for both projects

#pragma once
#include <windows.h>
#include <stdint.h>
#include <iostream>
#include <d3d11.h>
#include <wrl.h>


class DoubleBuffer
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

	// Инициализация с тремя событиями!
	bool Initialize(HANDLE hSharedMemory,
		HANDLE hFrameReadyEvent, // Драйвер -> Приложение: "новый кадр готов"
		HANDLE hFrameProcessedEvent, // Приложение -> Драйвер: "кадр обработан"
		uint16_t width,
		uint16_t height,
		uint16_t byteDepth);

	// Запись кадра (драйвер)
	void PushFrame(ID3D11Texture2D* pTexture, ID3D11DeviceContext* pContext, uint64_t frameId, uint64_t timestamp);

	// Чтение кадра (приложение)
	Frame GetLatestFrame();

	// Отметить кадр как обработанный (приложение)
	void MarkFrameProcessed(uint16_t bufferIndex);

	// Для драйвера - ждать освобождения буфера
	HANDLE GetFrameProcessedEvent() const;
	HANDLE GetFrameReadyEvent() const;

	// Очистка
	void Cleanup();
private:
	HANDLE m_hSharedMemory = nullptr;
	HANDLE m_hFrameReadyEvent = nullptr; // Драйвер -> Приложение
	HANDLE m_hFrameProcessedEvent = nullptr; // Приложение -> Драйвер
	void* m_pMappedBuffer = nullptr;

	uint16_t m_width = 0;
	uint16_t m_height = 0;
	uint16_t m_byteDepth = 0;
	uint32_t m_frameSize = 0;
	uint32_t m_headerSize = 0;

	uint32_t m_buffer0_offset = 0;
	uint32_t m_buffer1_offset = 0;
};

