#include "FrameManager.h"
#include <iostream>
#include <algorithm>
#include <cstring>

FrameManager::FrameManager(MonitorConfig& config, EncoderType type)
    : m_config(config)
{
    if (type == EncoderType::Jpeg) {
        m_encoder = std::make_unique<JpegEncoder>(config);
    }
    else {
        m_encoder = std::make_unique<FFmpegEncoder>(config);
    }
    m_packetPool.resize(MAX_PACKETS);
}

std::span<const SPacket> FrameManager::encodeFrame(
    uint64_t        frameId,
    uint32_t        rowPitch,
    const uint8_t* inputData,
    size_t          /*inputSize*/)
{
    if (!m_encoder) return {};

    // Вызываем полиморфный метод encode
    std::span<const uint8_t> encoded = m_encoder->encode(inputData, rowPitch);
    if (encoded.empty()) {
        std::cerr << "FrameManager: encode failed for frame " << frameId << "\n";
        return {};
    }

    const size_t   totalBytes = encoded.size();
    const uint32_t totalParts = static_cast<uint32_t>(
        (totalBytes + SPACKET_MAX_DATA_SIZE - 1) / SPACKET_MAX_DATA_SIZE);

    if (totalParts > MAX_PACKETS) {
        std::cerr << "FrameManager: encoded frame too large ("
            << totalBytes << " bytes, " << totalParts << " parts)\n";
        return {};
    }

    // Забираем тип пакета у кодера
    const uint16_t payloadType = m_encoder->getPayloadType();

    for (uint32_t i = 0; i < totalParts; ++i) {
        const uint32_t offset = i * SPACKET_MAX_DATA_SIZE;
        const uint16_t partSize = static_cast<uint16_t>(
            std::min<size_t>(SPACKET_MAX_DATA_SIZE, totalBytes - offset));

        SPacket& pkt = m_packetPool[i];
        pkt.type = payloadType; // Теперь тип динамический
        pkt.frameId = frameId;
        pkt.totalParts = static_cast<uint16_t>(totalParts);
        pkt.partId = static_cast<uint16_t>(i);
        pkt.dataOffset = offset;
        pkt.dataSize = partSize;
        std::memcpy(pkt.data, encoded.data() + offset, partSize);
    }

    return { m_packetPool.data(), totalParts };
}