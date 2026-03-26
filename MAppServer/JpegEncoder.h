#pragma once
#include "IEncoder.h"
#include "../Common.h"
#include <turbojpeg.h>

class JpegEncoder : public IEncoder
{
public:
    explicit JpegEncoder(const MonitorConfig& config);
    ~JpegEncoder() override;

    std::span<const uint8_t> encode(const uint8_t* bgraData, uint32_t rowPitch) override;
    uint16_t getPayloadType() const override { return SPACKET_TYPE_JPEG; } // Убедись, что SPACKET_TYPE_JPEG есть в SPacket.h

private:
    MonitorConfig m_config;
    tjhandle      m_tjInstance = nullptr;
    uint8_t* m_jpegBuf = nullptr;
    unsigned long m_jpegSize = 0;
};

