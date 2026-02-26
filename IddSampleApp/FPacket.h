#pragma once

#include <cstdint>
#include <vector>
#include <iostream>

#define FPACKET_HEADER_SIZE 16
#define MAX_FRAME_SIZE 65520 // (65536 - 16)

//Frame packet
struct FPacket {
    uint16_t type = 300;
    uint32_t frameId = 0;
    
    uint16_t totalParts = 0;
    uint16_t partId = 0;
    uint32_t partOffset = 0;
    uint16_t partSize = 0;
    uint8_t partData[MAX_FRAME_SIZE];

    std::vector<uint8_t> bytes() const {
        std::vector<uint8_t> byteArray(sizeof(FPacket), 0);
        std::memcpy(byteArray.data(), this, sizeof(FPacket));
        return byteArray;
    }

    //static FPacket fromBytes(const std::vector<uint8_t>& data) {
    //    FPacket packet;

    //    if (data.size() < sizeof(FPacket)) {
    //        std::cerr << "Not enough data to reconstruct APacket";
    //    }

    //    std::memcpy(&packet, data.data(), FPACKET_HEADER_SIZE);

    //    return packet;
    //}
};