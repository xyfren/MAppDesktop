#include "JpegCoder.h"

JpegCoder::JpegCoder(MonitorConfig& config) :
	m_config(config)
{
	m_compressor = tjInitCompress();
}

int JpegCoder::encodeToJpeg(const uint8_t* inputBuffer, uint8_t** outputBuffer, unsigned long * outputSize) {
    // ВАЖНО: Если *outBuf == nullptr, TurboJPEG выделит память сам.
    // Если *outBuf уже указывает на память, TurboJPEG попытается её переиспользовать 
    // или перевыделить (realloc), если её не хватит.

    int result = tjCompress2(
        m_compressor,
        inputBuffer,
        m_config.width,
        0,                 // Замени на RowPitch, если данные из DX11!
        m_config.height,
        TJPF_BGRA,
        outputBuffer,            // Двойной указатель: сюда запишется адрес новой памяти
        outputSize,           // Сюда запишется итоговый размер
        TJSAMP_420,
        60,
        TJFLAG_FASTDCT | TJFLAG_NOREALLOC   // Убираем NOREALLOC, чтобы позволить выделение
    );

    return result; // 0 — успех
}

JpegCoder::~JpegCoder()
{
	tjDestroy(m_compressor);
}