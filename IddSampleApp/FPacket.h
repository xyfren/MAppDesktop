#pragma once

#include <cstdint>
#include <vector>
#include <iostream>

#define FPACKET_HEADER_SIZE 20
// Keep each UDP datagram well below the typical Ethernet MTU of 1500 bytes to
// avoid IP-level fragmentation.  A single lost IP fragment drops the entire
// datagram, which was the primary source of frame losses with the old 16384-
// byte payload.  Total on-wire size: 20 (FPacket header) + 1300 (data) +
// 8 (UDP) + 20 (IP) = 1348 bytes — safely below 1500.
#define FPACKET_MAX_FRAME_SIZE 1300

//Frame packet
// Packet type IDs sent in FPacket::type.
#define FPACKET_TYPE_H264  310  // Frame data is H.264 NAL units (current codec)

#pragma pack(push,1)
struct FPacket {
    uint16_t type = FPACKET_TYPE_H264;
    uint64_t frameId = 0;
    
    uint16_t totalParts = 0;
    uint16_t partId = 0;
    uint32_t partOffset = 0;
    uint16_t partSize = 0;
    uint8_t partData[FPACKET_MAX_FRAME_SIZE];

    std::vector<uint8_t> bytes() const {
        std::vector<uint8_t> byteArray(sizeof(FPacket), 0);
        std::memcpy(byteArray.data(), this, sizeof(FPacket));
        return byteArray;
    }

    const uint8_t* rawData() const {
        return reinterpret_cast<const uint8_t*>(this);
    }
};
#pragma pack(pop)