#include "JpegEncoder.h"
#include <iostream>

JpegEncoder::JpegEncoder(const MonitorConfig& config)
    : m_config(config)
{
    m_tjInstance = tjInitCompress();
    if (!m_tjInstance) {
        std::cerr << "JpegEncoder: tjInitCompress failed\n";
    }

    // Предварительно выделяем буфер максимального размера, чтобы избежать реаллокаций при стриминге
    unsigned long maxSize = tjBufSize(m_config.width, m_config.height, TJSAMP_420);
    m_jpegBuf = tjAlloc(maxSize);
}

JpegEncoder::~JpegEncoder()
{
    if (m_tjInstance) tjDestroy(m_tjInstance);
    if (m_jpegBuf)    tjFree(m_jpegBuf);
}

std::span<const uint8_t> JpegEncoder::encode(const uint8_t* bgraData, uint32_t rowPitch)
{
    if (!m_tjInstance || !m_jpegBuf) return {};

    m_jpegSize = 0;

    // Настройки сжатия (качество 80, субсемплинг 4:2:0)
    const int jpegQual = 50;
    const int flags = TJFLAG_NOREALLOC | TJFLAG_FASTUPSAMPLE;

    int ret = tjCompress2(
        m_tjInstance,
        bgraData,
        m_config.width,
        rowPitch,
        m_config.height,
        TJPF_BGRA,     // Ожидаемый формат пикселей на входе
        &m_jpegBuf,
        &m_jpegSize,
        TJSAMP_420,    // Цветовая субдискретизация
        jpegQual,
        flags
    );

    if (ret < 0) {
        std::cerr << "JpegEncoder: compress failed: " << tjGetErrorStr2(m_tjInstance) << "\n";
        return {};
    }

    return { m_jpegBuf, static_cast<size_t>(m_jpegSize) };
}