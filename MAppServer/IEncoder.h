#pragma once
#include <cstdint>
#include <span>
#include "SPacket.h"

class IEncoder
{
public:
    virtual ~IEncoder() = default;

    virtual std::span<const uint8_t> encode(const uint8_t* bgraData, uint32_t rowPitch) = 0;

    virtual uint16_t getPayloadType() const = 0;
};