#pragma once

#include <cstdint>
#include <vector>
#include <iostream>

#pragma pack(push, 1)
struct DPacket {
    uint16_t type = 200;

    std::vector<uint8_t> bytes() const {
        std::vector<uint8_t> byteArray(sizeof(DPacket), 0);
        std::memcpy(byteArray.data(), this, sizeof(DPacket));
        return byteArray;
    }

    static DPacket fromBytes(const std::vector<uint8_t>& data) {
        DPacket packet;

        if (data.size() < sizeof(DPacket)) {
            std::cerr << "Not enough data to reconstruct APacket";
        }

        std::memcpy(&packet, data.data(), sizeof(DPacket));
        return packet;
    }
};

struct RDPacket {
    uint16_t type = 201;
    uint16_t response = 0;
    uint32_t ipAddress = 0;
    uint16_t port = 0;

    std::vector<uint8_t> bytes() const {
        std::vector<uint8_t> byteArray(sizeof(DPacket), 0);
        std::memcpy(byteArray.data(), this, sizeof(DPacket));
        return byteArray;
    }

    static DPacket fromBytes(const std::vector<uint8_t>& data) {
        DPacket packet;

        if (data.size() < sizeof(DPacket)) {
            std::cerr << "Not enough data to reconstruct APacket";
        }

        std::memcpy(&packet, data.data(), sizeof(DPacket));
        return packet;
    }
};

#pragma pack(pop)