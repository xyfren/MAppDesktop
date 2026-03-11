#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <cstdint>
#include <cstddef>
#include "../Common.h"

// Encodes raw BGRA frames to H.264 using FFmpeg (libx264).
//
// Every frame is encoded as an intra (I) frame so that each encoded packet can
// be decoded independently on the receiver side — matching the JPEG semantics
// of the original JpegCoder but with significantly better compression.
//
// The receiver must be updated to decode H.264 data instead of JPEG.
class FFmpegCoder
{
public:
    explicit FFmpegCoder(const MonitorConfig& config);
    ~FFmpegCoder();

    // Encode one BGRA frame.
    //
    // inputBgraData : pointer to BGRA pixel data.
    // rowPitch      : bytes per row (may be > width*4 due to GPU alignment).
    // outputBuffer  : on entry, *outputBuffer points to a caller-owned buffer of
    //                 *outputSize bytes.  On success, *outputBuffer is updated to
    //                 point to the encoded H.264 data and *outputSize is set to
    //                 the number of bytes written.  The returned pointer remains
    //                 valid until the next call to encodeFrame().
    //
    // Returns 0 on success, negative FFmpeg error code on failure.
    int encodeFrame(const uint8_t* inputBgraData, uint32_t rowPitch,
                    uint8_t** outputBuffer, size_t* outputSize);

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
