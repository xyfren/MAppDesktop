#pragma once

#include <cstdint>
#include <cstddef>

// Each SPacket payload fits comfortably inside a single Ethernet MTU (1500 bytes).
// On-wire size: SPACKET_HEADER_SIZE (20) + 1300 (data) + 8 (UDP) + 20 (IP) = 1348 bytes.
#define SPACKET_MAX_DATA_SIZE 1300
#define SPACKET_HEADER_SIZE   20
#define SPACKET_TYPE_H264     320

// SPacket (StreamPacket) — UDP datagram carrying one fragment of an encoded video frame.
// Fragments are numbered 0..totalParts-1; the receiver reassembles them using dataOffset.
#pragma pack(push, 1)
struct SPacket {
    uint16_t type       = SPACKET_TYPE_H264; // Packet type identifier
    uint64_t frameId    = 0;                 // Monotonically increasing frame counter
    uint16_t totalParts = 0;                 // Total number of fragments for this frame
    uint16_t partId     = 0;                 // Zero-based index of this fragment
    uint32_t dataOffset = 0;                 // Byte offset of this fragment in the full frame
    uint16_t dataSize   = 0;                 // Number of valid bytes in data[]
    uint8_t  data[SPACKET_MAX_DATA_SIZE];    // Encoded payload bytes

    // Pointer to the start of the on-wire representation (header + payload).
    const uint8_t* rawData() const {
        return reinterpret_cast<const uint8_t*>(this);
    }

    // Size of the header fields (everything before data[]).
    static constexpr size_t headerSize() {
        return SPACKET_HEADER_SIZE;
    }
};
#pragma pack(pop)

static_assert(offsetof(SPacket, data) == SPACKET_HEADER_SIZE,
              "SPacket header size does not match SPACKET_HEADER_SIZE");
