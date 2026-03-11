#include "FrameManager.h"

#include <iostream>
#include <algorithm>
#include <cstring>

FrameManager::FrameManager(const MonitorConfig& config)
    : m_encoder(config)
{
    // Pre-allocate the packet pool once — no per-frame heap allocation.
    m_packetPool.resize(MAX_PACKETS);
}

std::span<const SPacket> FrameManager::encodeFrame(
    uint64_t        frameId,
    uint32_t        rowPitch,
    const uint8_t*  inputData,
    size_t          /*inputSize*/)
{
    // Encode BGRA -> H.264 (zero-copy: returns view into FFmpeg's internal buffer).
    std::span<const uint8_t> encoded = m_encoder.encode(inputData, rowPitch);
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

    // Fill pre-allocated SPackets — one memcpy per fragment, no extra allocation.
    for (uint32_t i = 0; i < totalParts; ++i) {
        const uint32_t offset   = i * SPACKET_MAX_DATA_SIZE;
        const uint16_t partSize = static_cast<uint16_t>(
            std::min<size_t>(SPACKET_MAX_DATA_SIZE, totalBytes - offset));

        SPacket& pkt    = m_packetPool[i];
        pkt.type        = SPACKET_TYPE_H264;
        pkt.frameId     = frameId;
        pkt.totalParts  = static_cast<uint16_t>(totalParts);
        pkt.partId      = static_cast<uint16_t>(i);
        pkt.dataOffset  = offset;
        pkt.dataSize    = partSize;
        std::memcpy(pkt.data, encoded.data() + offset, partSize);
    }

    return { m_packetPool.data(), totalParts };
}
