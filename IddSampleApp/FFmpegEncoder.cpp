#include "FFmpegEncoder.h"

#include <iostream>

FFmpegEncoder::FFmpegEncoder(const MonitorConfig& config)
    : m_config(config)
{
    if (!initialize()) {
        std::cerr << "FFmpegEncoder: initialization failed\n";
    }
}

FFmpegEncoder::~FFmpegEncoder()
{
    cleanup();
}

bool FFmpegEncoder::initialize()
{
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) return false;

    m_codecCtx = avcodec_alloc_context3(codec);

    // 1. Базовые параметры
    m_codecCtx->width = m_config.width;
    m_codecCtx->height = m_config.height;
    m_codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_codecCtx->time_base = { 1, 60 };
    m_codecCtx->framerate = { 60, 1 };

    // 2. Убираем задержку буферизации (КРИТИЧНО)
    m_codecCtx->gop_size = 1;       // Каждый кадр - I-frame (Intra-only)
    m_codecCtx->max_b_frames = 0;   // B-кадры создают задержку, они нам не нужны
    m_codecCtx->thread_count = 1;   // Один поток гарантирует "кадр на вход -> пакет на выход"
    m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    // 3. Опции x264
    av_opt_set(m_codecCtx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(m_codecCtx->priv_data, "tune", "zerolatency", 0);

    // 4. Битрейт (без него x264 может выдавать пустые пакеты на статичной картинке)
    

    AVDictionary* param = nullptr;

    av_dict_set(&param, "preset", "p1", 0);
    av_dict_set(&param, "tune", "ull", 0);
    av_dict_set(&param, "delay", "0", 0);

    // Управление битрейтом (Constant Bitrate лучше для сети)
    m_codecCtx->bit_rate = 4000000; // 4 Mbps для начала
    m_codecCtx->rc_max_rate = 20000000;
    m_codecCtx->rc_buffer_size = 0; // Немедленная отправка без накопления

    // Принудительные IDR кадры для возможности мгновенного декодирования
    av_dict_set(&param, "forced-idr", "1", 0);

    int ret = avcodec_open2(m_codecCtx, codec, &param);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "FFmpegEncoder: open failed: " << errbuf << "\n";
        return false;
    }
    av_dict_free(&param);

    // Allocate the YUV frame that the encoder will consume.
    m_yuvFrame = av_frame_alloc();
    if (!m_yuvFrame) return false;
    m_yuvFrame->format = AV_PIX_FMT_YUV420P;
    m_yuvFrame->width  = m_config.width;
    m_yuvFrame->height = m_config.height;
    if (av_frame_get_buffer(m_yuvFrame, 0) < 0) {
        std::cerr << "FFmpegEncoder: av_frame_get_buffer failed\n";
        return false;
    }

    m_packet = av_packet_alloc();
    if (!m_packet) return false;

    // Color-space conversion context: BGRA (GPU output) -> YUV420PP (encoder input).
    m_swsCtx = sws_getContext(
        m_config.width,  m_config.height, AV_PIX_FMT_BGRA,
        m_config.width,  m_config.height, AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsCtx) {
        std::cerr << "FFmpegEncoder: sws_getContext failed\n";
        return false;
    }

    return true;
}

void FFmpegEncoder::cleanup()
{
    if (m_swsCtx)   { sws_freeContext(m_swsCtx);        m_swsCtx   = nullptr; }
    if (m_packet)   { av_packet_free(&m_packet);         m_packet   = nullptr; }
    if (m_yuvFrame) { av_frame_free(&m_yuvFrame);        m_yuvFrame = nullptr; }
    if (m_codecCtx) { avcodec_free_context(&m_codecCtx); m_codecCtx = nullptr; }
}

std::span<const uint8_t> FFmpegEncoder::encode(const uint8_t* bgraData, uint32_t rowPitch)
{
    
    if (!m_codecCtx || !m_yuvFrame || !m_swsCtx || !m_packet) {
        std::cout << "error encode" << std::endl;
        return {};
    }

    // Convert BGRA -> YUV420PP in-place (no extra allocation).
    if (av_frame_make_writable(m_yuvFrame) < 0) {

        std::cout << "error conv" << std::endl;
        return {};
    }
    const uint8_t* srcSlice[1]  = { bgraData };
    int            srcStride[1] = { static_cast<int>(rowPitch) };
    sws_scale(m_swsCtx, srcSlice, srcStride, 0, m_config.height,
              m_yuvFrame->data, m_yuvFrame->linesize);

    m_yuvFrame->pts = m_pts++;

    // Submit the frame to the encoder.
    if (avcodec_send_frame(m_codecCtx, m_yuvFrame) < 0) {
        std::cout << "error avcodec_send_frame" << std::endl;
        return {};
    }


    m_encodedBuffer.clear();
    for (;;) {
        int ret = avcodec_receive_packet(m_codecCtx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            std::cout << "error avcodec_receive_packet" << std::endl;
            return {};
        }
        m_encodedBuffer.insert(m_encodedBuffer.end(),
                               m_packet->data,
                               m_packet->data + m_packet->size);
        av_packet_unref(m_packet);
    }

    if (m_encodedBuffer.empty()) {
        return {};
    }

    return { m_encodedBuffer.data(), m_encodedBuffer.size() };
}
