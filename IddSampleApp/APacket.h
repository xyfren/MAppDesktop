#pragma once
#include <cstdint>
#include <vector>
#include <iostream>

#pragma pack(push, 1)
struct APacket {
    uint16_t type = 100;
    uint16_t udpPort = 0;

    std::vector<uint8_t> bytes() const {
        std::vector<uint8_t> byteArray(sizeof(APacket), 0);
        std::memcpy(byteArray.data(), this, sizeof(APacket));
        return byteArray;
    }

    static APacket fromBytes(const std::vector<uint8_t>& data) {
        APacket packet;

        if (data.size() < sizeof(APacket)) {
            std::cerr << "Not enough data to reconstruct APacket";
        }

        std::memcpy(&packet, data.data(), sizeof(APacket));
        return packet;
    }
};

struct RAPacket {
    uint16_t type = 101;
    uint16_t response = 0;

    std::vector<uint8_t> bytes() const {
        std::vector<uint8_t> byteArray(sizeof(RAPacket), 0);
        std::memcpy(byteArray.data(), this, sizeof(RAPacket));
        return byteArray;
    }

    static RAPacket fromBytes(const std::vector<uint8_t>& data) {
        RAPacket packet;

        if (data.size() < sizeof(RAPacket)) {
            std::cerr << "Not enough data to reconstruct RAPacket";
        }

        std::memcpy(&packet, data.data(), sizeof(RAPacket));
        return packet;
    }
};

#pragma pack(pop)

