#include "JpegCoder.h"

JpegCoder::JpegCoder(MonitorConfig& config) :
	m_config(config)
{
	m_compressor = tjInitCompress();
}

int JpegCoder::encodeToJpeg(const uint8_t* inputBuffer, uint32_t rowPitch, uint8_t** outputBuffer, unsigned long * outputSize) {
    int result = tjCompress2(
        m_compressor,
        inputBuffer,
        m_config.width,
        rowPitch,                 // Замени на RowPitch, если данные из DX11!
        m_config.height,
        TJPF_BGRA,
        outputBuffer,            // Двойной указатель: сюда запишется адрес новой памяти
        outputSize,           // Сюда запишется итоговый размер
        TJSAMP_420,
        75,
        TJFLAG_FASTDCT | TJFLAG_NOREALLOC   // NOREALLOC убран: разрешаем TurboJPEG перевыделять буфер при необходимости
    );

    return result; // 0 — успех
}

JpegCoder::~JpegCoder()
{
	tjDestroy(m_compressor);
}