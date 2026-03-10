#include "FFmpegCoder.h"

#include <iostream>
#include <cstring>

FFmpegCoder::FFmpegCoder(const MonitorConfig& config)
    : m_config(config)
{
    if (!initialize()) {
        std::cerr << "FFmpegCoder: initialization failed\n";
    }
}

FFmpegCoder::~FFmpegCoder()
{
    cleanup();
}

bool FFmpegCoder::initialize()
{
    // Prefer the software libx264 encoder for maximum portability and control.
    // If libx264 is not available in the FFmpeg build, fall back to any
    // registered H.264 encoder.
    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (!codec) {
        std::cerr << "FFmpegCoder: H.264 encoder not found\n";
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        std::cerr << "FFmpegCoder: avcodec_alloc_context3 failed\n";
        return false;
    }

    m_codecCtx->width        = m_config.width;
    m_codecCtx->height       = m_config.height;
    m_codecCtx->pix_fmt      = AV_PIX_FMT_YUV420P;
    int fps                  = m_config.refreshRate > 0 ? m_config.refreshRate : 60;
    m_codecCtx->time_base    = { 1, fps };
    m_codecCtx->framerate    = { fps, 1 };
    // Intra-only: every frame is a keyframe so the receiver can always decode
    // the latest frame without needing previous frames (no inter-frame state).
    m_codecCtx->gop_size     = 0;
    m_codecCtx->max_b_frames = 0;

    // Low-latency libx264 settings.
    av_opt_set(m_codecCtx->priv_data, "preset",     "ultrafast",           0);
    av_opt_set(m_codecCtx->priv_data, "tune",       "zerolatency",         0);
    // Force all frames to be I-frames and disable scene-cut detection.
    av_opt_set(m_codecCtx->priv_data, "x264-params","keyint=1:scenecut=0", 0);

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        std::cerr << "FFmpegCoder: avcodec_open2 failed\n";
        return false;
    }

    // Allocate the YUV frame that the encoder will consume.
    m_yuvFrame = av_frame_alloc();
    if (!m_yuvFrame) return false;
    m_yuvFrame->format = AV_PIX_FMT_YUV420P;
    m_yuvFrame->width  = m_config.width;
    m_yuvFrame->height = m_config.height;
    if (av_frame_get_buffer(m_yuvFrame, 0) < 0) {
        std::cerr << "FFmpegCoder: av_frame_get_buffer failed\n";
        return false;
    }

    m_packet = av_packet_alloc();
    if (!m_packet) return false;

    // Color-space conversion context: BGRA (GPU output) → YUV420P (encoder input).
    m_swsCtx = sws_getContext(
        m_config.width,  m_config.height, AV_PIX_FMT_BGRA,
        m_config.width,  m_config.height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsCtx) {
        std::cerr << "FFmpegCoder: sws_getContext failed\n";
        return false;
    }

    return true;
}

void FFmpegCoder::cleanup()
{
    if (m_swsCtx)   { sws_freeContext(m_swsCtx);        m_swsCtx   = nullptr; }
    if (m_packet)   { av_packet_free(&m_packet);         m_packet   = nullptr; }
    if (m_yuvFrame) { av_frame_free(&m_yuvFrame);        m_yuvFrame = nullptr; }
    if (m_codecCtx) { avcodec_free_context(&m_codecCtx); m_codecCtx = nullptr; }
}

int FFmpegCoder::encodeFrame(const uint8_t* inputBgraData, uint32_t rowPitch,
                              uint8_t** outputBuffer, size_t* outputSize)
{
    if (!m_codecCtx || !m_yuvFrame || !m_swsCtx || !m_packet) {
        return AVERROR(EINVAL);
    }

    // Convert BGRA → YUV420P.
    if (av_frame_make_writable(m_yuvFrame) < 0) {
        return AVERROR(EINVAL);
    }
    const uint8_t* srcSlice[1] = { inputBgraData };
    int            srcStride[1] = { static_cast<int>(rowPitch) };
    sws_scale(m_swsCtx, srcSlice, srcStride, 0, m_config.height,
              m_yuvFrame->data, m_yuvFrame->linesize);

    m_yuvFrame->pts = m_pts++;

    // Encode.
    int ret = avcodec_send_frame(m_codecCtx, m_yuvFrame);
    if (ret < 0) {
        std::cerr << "FFmpegCoder: avcodec_send_frame error " << ret << "\n";
        return ret;
    }

    ret = avcodec_receive_packet(m_codecCtx, m_packet);
    if (ret < 0) {
        std::cerr << "FFmpegCoder: avcodec_receive_packet error " << ret << "\n";
        return ret;
    }

    // Copy encoded data into the caller's buffer so we can safely unref the
    // packet before the next call.
    if (m_packet->size > 0) {
        if (static_cast<size_t>(m_packet->size) > *outputSize) {
            // Grow the caller's buffer.  Use realloc so it stays heap-allocated
            // in a way compatible with subsequent free() calls.
            uint8_t* newBuf = static_cast<uint8_t*>(realloc(*outputBuffer,
                                                             m_packet->size));
            if (!newBuf) {
                av_packet_unref(m_packet);
                return AVERROR(ENOMEM);
            }
            *outputBuffer = newBuf;
        }
        std::memcpy(*outputBuffer, m_packet->data, m_packet->size);
        *outputSize = static_cast<size_t>(m_packet->size);
    }

    av_packet_unref(m_packet);
    return 0;
}
