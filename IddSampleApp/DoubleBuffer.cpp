#include "../DoubleBuffer.h"

// Here DoubleBuffer realization for App

bool DoubleBuffer::Initialize(HANDLE hSharedMemory,
    HANDLE hFrameReadyEvent, // Драйвер -> Приложение: "новый кадр готов"
    HANDLE hFrameProcessedEvent, // Приложение -> Драйвер: "кадр обработан"
    uint16_t width,
    uint16_t height,
    uint16_t byteDepth)
{

    HANDLE hCurrentProcess = GetCurrentProcess();

    m_hSharedMemory = hSharedMemory;
    m_hFrameReadyEvent = hFrameReadyEvent;
    m_hFrameProcessedEvent = hFrameProcessedEvent;

    // Маппим память
    m_pMappedBuffer = MapViewOfFile(m_hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!m_pMappedBuffer) return false;

    // Предвычисляем смещения
    m_width = width;
    m_height = height;
    m_byteDepth = byteDepth;
    m_frameSize = width * height * byteDepth;
    m_headerSize = sizeof(FrameHeader);
    m_buffer0_offset = m_headerSize;
    m_buffer1_offset = m_headerSize + m_frameSize;

    // Инициализируем заголовок
    FrameHeader* header = (FrameHeader*)m_pMappedBuffer;
    header->width = width;
    header->height = height;
    header->byteDepth = byteDepth;
    header->format = 87;
    header->frameSize = m_frameSize;
    header->bufferIndex = 0;
    header->bufferProccesed[0] = false;
    header->bufferProccesed[1] = false;

    return true;
}

// Driver only
void DoubleBuffer::PushFrame(ID3D11Texture2D* pTexture, ID3D11DeviceContext* pContext, uint64_t frameId, uint64_t timestamp)
{
    if (!m_pMappedBuffer) return;

    FrameHeader* header = (FrameHeader*)m_pMappedBuffer;

    // Выбираем буфер для записи
    uint32_t currentOffset = ((header->bufferIndex ^ 1) == 0) ? m_buffer0_offset : m_buffer1_offset;

    // Маппим текстуру
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(pContext->Map(pTexture, 0, D3D11_MAP_READ, 0, &mapped))) {

        // Копируем пиксели
        uint16_t pitch = m_width * m_byteDepth;

        uint8_t* dst = (uint8_t*)m_pMappedBuffer + currentOffset;
        uint8_t* src = (uint8_t*)mapped.pData;

        if (mapped.RowPitch == pitch) {
            memcpy(dst, src, m_frameSize);
        }
        else {
            for (UINT y = 0; y < m_height; y++) {
                memcpy(dst + y * pitch, src + y * mapped.RowPitch, pitch);
            }
        }

        pContext->Unmap(pTexture, 0);

        // Обновляем заголовок
        header->frameId = frameId;
        header->timestamp = timestamp;
        header->bufferIndex ^= 1; // Какой буфер свежий

        // Сигнал приложению: "НОВЫЙ КАДР ГОТОВ!"
        SetEvent(m_hFrameReadyEvent);
    }
}

// App only
DoubleBuffer::Frame DoubleBuffer::GetLatestFrame()
{
    FrameHeader* header = (FrameHeader*)m_pMappedBuffer;

    // Выбираем свежий буфер
    uint32_t readOffset = (header->bufferIndex == 0) ? m_buffer0_offset : m_buffer1_offset;
    //std::cout << header->frameSize << '\n';
    //std::cout << header->bufferIndex << '\n';
    Frame frame;
    frame.pixels = (uint8_t*)m_pMappedBuffer + readOffset;
    frame.size = header->frameSize;
    frame.frameId = header->frameId;
    frame.timestamp = header->timestamp;
    frame.width = header->width;
    frame.height = header->height;
    frame.byteDepth = header->byteDepth;
    frame.bufferIndex = header->bufferIndex;
    return frame;
}

// App only
void DoubleBuffer::MarkFrameProcessed(uint16_t bufferIndex)
{
    FrameHeader* header = (FrameHeader*)m_pMappedBuffer;
    if (header->bufferProccesed[bufferIndex])
        header->bufferProccesed[bufferIndex] = false; // Освобождаем буфер
    else
        header->bufferProccesed[bufferIndex] = true;
    SetEvent(m_hFrameProcessedEvent); // Сигнал драйверу
}

HANDLE DoubleBuffer::GetFrameProcessedEvent() const
{
    return m_hFrameProcessedEvent;
}

HANDLE DoubleBuffer::GetFrameReadyEvent() const
{
    return m_hFrameReadyEvent;
}

void DoubleBuffer::Cleanup()
{
    if (m_pMappedBuffer) UnmapViewOfFile(m_pMappedBuffer);
    if (m_hSharedMemory) CloseHandle(m_hSharedMemory);
    if (m_hFrameReadyEvent) CloseHandle(m_hFrameReadyEvent);
    if (m_hFrameProcessedEvent) CloseHandle(m_hFrameProcessedEvent);

    m_pMappedBuffer = nullptr;
    m_hSharedMemory = nullptr;
    m_hFrameReadyEvent = nullptr;
    m_hFrameProcessedEvent = nullptr;
}
