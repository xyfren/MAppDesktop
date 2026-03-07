#pragma once

#include <cstdint>
#include <vector>
#include <iostream>

#define FPACKET_HEADER_SIZE 20
#define FPACKET_MAX_FRAME_SIZE 32768 // (65536 - 16)

//Frame packet
#pragma pack(push,1)
struct FPacket {
    uint16_t type = 300;
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