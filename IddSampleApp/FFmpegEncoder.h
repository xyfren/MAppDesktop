#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <cstdint>
#include <span>
#include <vector>
#include "../Common.h"
#include "IEncoder.h"

class FFmpegEncoder : public IEncoder
{
public:
    explicit FFmpegEncoder(const MonitorConfig& config);
    ~FFmpegEncoder();


    std::span<const uint8_t> encode(const uint8_t* bgraData, uint32_t rowPitch) override;

    uint16_t getPayloadType() const override { return SPACKET_TYPE_H264; }
private:
    bool initialize();
    void cleanup();

    MonitorConfig   m_config;
    AVCodecContext* m_codecCtx  = nullptr;
    AVFrame*        m_yuvFrame  = nullptr;
    AVPacket*       m_packet    = nullptr;
    SwsContext*     m_swsCtx    = nullptr;
    int64_t         m_pts       = 0;

    // Buffer that accumulates encoded data from all packets produced by a
    // single avcodec_send_frame() call.  Reused across encode() invocations
    // to avoid per-frame heap allocation after the first few frames.
    std::vector<uint8_t> m_encodedBuffer;
};
