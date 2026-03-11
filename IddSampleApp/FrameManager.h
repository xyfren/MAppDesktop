#pragma once

#include <vector>
#include <span>
#include <cstdint>

#include "FFmpegEncoder.h"
#include "SPacket.h"
#include "../Common.h"

// FrameManager encodes a raw BGRA frame to H.264 and fragments it into
// SPackets ready for UDP transmission.
//
// All SPackets are stored in a pre-allocated pool so that no heap allocation
// occurs per frame.  The returned span is valid until the next call to
// encodeFrame() or until this object is destroyed.
class FrameManager
{
public:
    explicit FrameManager(const MonitorConfig& config);

    // Encode one BGRA frame and fill the internal SPacket pool.
    //
    // frameId   : monotonically increasing frame counter.
    // rowPitch  : bytes per row (may exceed width*4 due to GPU alignment).
    // inputData : pointer to BGRA pixel data.
    // inputSize : total byte size of inputData (used only for validation).
    //
    // Returns a span over the pre-allocated SPacket pool containing the
    // fragmented encoded frame.  Returns an empty span on failure.
    std::span<const SPacket> encodeFrame(uint64_t frameId, uint32_t rowPitch,
                                         const uint8_t* inputData, size_t inputSize);

private:
    FFmpegEncoder m_encoder;

    // Pre-allocated pool — sized for the largest possible encoded frame.
    // 2048 * 1300 bytes payload = ~2.6 MB max; adequate for any resolution
    // at "ultrafast" + intra-only settings.
    static constexpr size_t MAX_PACKETS = 2048;
    static_assert(MAX_PACKETS <= 0xFFFF,
                  "MAX_PACKETS must fit in uint16_t (SPacket::totalParts / partId)");
    std::vector<SPacket> m_packetPool;
};
