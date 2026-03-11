#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <cstdint>
#include <span>
#include "../Common.h"

// Encodes raw BGRA frames to H.264 using FFmpeg (libx264).
//
// Every frame is encoded as an intra (I-frame) so that each encoded packet can
// be decoded independently — no inter-frame state is required on the receiver.
//
// The encode() method returns a span pointing directly into the internal
// AVPacket buffer.  No external allocation or memcpy is performed; the span is
// valid until the next call to encode().
class FFmpegEncoder
{
public:
    explicit FFmpegEncoder(const MonitorConfig& config);
    ~FFmpegEncoder();

    // Encode one BGRA frame.
    //
    // bgraData  : pointer to BGRA pixel data (width * height * 4 bytes minimum).
    // rowPitch  : bytes per row (may be > width*4 due to GPU alignment).
    //
    // Returns a non-owning span pointing to the encoded H.264 NAL data inside
    // the internal AVPacket.  The span is valid until the next call to encode()
    // or until this object is destroyed.  Returns an empty span on failure.
    std::span<const uint8_t> encode(const uint8_t* bgraData, uint32_t rowPitch);

private:
    bool initialize();
    void cleanup();

    MonitorConfig   m_config;
    AVCodecContext* m_codecCtx  = nullptr;
    AVFrame*        m_yuvFrame  = nullptr;
    AVPacket*       m_packet    = nullptr;
    SwsContext*     m_swsCtx    = nullptr;
    int64_t         m_pts       = 0;
};
