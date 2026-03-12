#pragma once

#include <vector>
#include <span>
#include <cstdint>
#include <memory>

#include "IEncoder.h"
#include "FFmpegEncoder.h"
#include "JpegEncoder.h"
#include "SPacket.h"
#include "../Common.h"

enum class EncoderType {
    FFmpeg,
    Jpeg
};

class FrameManager
{
public:
    // Принимаем любой энкодер через умный указатель
    explicit FrameManager(MonitorConfig& config,EncoderType type = EncoderType::Jpeg);

    std::span<const SPacket> encodeFrame(uint64_t frameId, uint32_t rowPitch,
        const uint8_t* inputData, size_t inputSize);

private:
    std::unique_ptr<IEncoder> m_encoder;

    MonitorConfig m_config;

    static constexpr size_t MAX_PACKETS = 2048;
    static_assert(MAX_PACKETS <= 0xFFFF,
                  "MAX_PACKETS must fit in uint16_t (SPacket::totalParts / partId)");
    std::vector<SPacket> m_packetPool;
};
