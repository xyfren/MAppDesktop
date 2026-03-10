#include "FrameManager.h"

#include <QDebug>
#include <cstring>

// ─── Constructor / Destructor ─────────────────────────────────────────────────

FrameManager::FrameManager(int width, int height, QObject *parent)
    : QObject(parent), m_width(width), m_height(height)
{
    if (!initDecoder()) {
        qCritical() << "FrameManager: H.264 decoder initialisation failed";
    }
}

FrameManager::~FrameManager()
{
    cleanupDecoder();
}

// ─── Public API ───────────────────────────────────────────────────────────────

void FrameManager::setResolution(int width, int height)
{
    if (width == m_width && height == m_height) return;

    cleanupDecoder();
    m_partialFrames.clear();
    m_width  = width;
    m_height = height;

    if (!initDecoder()) {
        qCritical() << "FrameManager: decoder re-init failed for"
                    << width << "x" << height;
    }
}

void FrameManager::processPacket(const QByteArray &data)
{
    // Minimum viable datagram: full header must be present.
    if (data.size() < FPACKET_HEADER_SIZE) return;

    const FPacketHeader *hdr =
        reinterpret_cast<const FPacketHeader *>(data.constData());

    // Ignore non-H.264 packets (e.g. control packets on the same socket).
    if (hdr->type != FPACKET_TYPE_H264) return;

    // Basic sanity checks on fragment metadata.
    if (hdr->totalParts == 0 || hdr->partSize == 0) return;
    if (hdr->partId >= hdr->totalParts) return;
    if (data.size() < FPACKET_HEADER_SIZE + hdr->partSize) return;

    const quint64 frameId    = hdr->frameId;
    const quint16 totalParts = hdr->totalParts;
    const quint16 partId     = hdr->partId;
    const quint32 offset     = hdr->partOffset;
    const quint16 size       = hdr->partSize;

    // Remove stale incomplete frames to bound memory usage.
    dropStaleFrames(frameId);

    PartialFrame &pf = m_partialFrames[frameId];
    if (pf.receivedParts == 0) {
        // First fragment seen for this frame — initialise the entry.
        pf.totalParts = totalParts;
        // Pre-allocate enough space for all parts at maximum payload size so
        // random-order arrival still lands in the right place.
        pf.buffer.resize(static_cast<int>(totalParts) * FPACKET_MAX_FRAME_SIZE);
    }

    // Guard against offset going past the buffer (shouldn't happen with a
    // well-behaved server, but prevents a crash if the server is mismatched).
    const int endByte = static_cast<int>(offset) + static_cast<int>(size);
    if (endByte > pf.buffer.size()) {
        pf.buffer.resize(endByte);
    }

    std::memcpy(pf.buffer.data() + offset,
                data.constData() + FPACKET_HEADER_SIZE,
                size);
    pf.receivedParts++;

    if (pf.receivedParts == pf.totalParts) {
        // All fragments received: trim to the actual data size and decode.
        // partOffset of the last fragment + its partSize gives the true size.
        const int trueSize = static_cast<int>(offset) + static_cast<int>(size);
        pf.buffer.resize(trueSize);

        decodeFrame(pf.buffer);
        m_partialFrames.remove(frameId);
    }
}

// ─── Private helpers ──────────────────────────────────────────────────────────

void FrameManager::dropStaleFrames(quint64 currentFrameId)
{
    if (currentFrameId < kStaleWindowSize) return; // avoid underflow at startup

    const quint64 threshold = currentFrameId - kStaleWindowSize;
    for (auto it = m_partialFrames.begin(); it != m_partialFrames.end(); ) {
        if (it.key() < threshold) {
            it = m_partialFrames.erase(it);
        } else {
            ++it;
        }
    }
}

bool FrameManager::initDecoder()
{
    // ── Codec ──────────────────────────────────────────────────────────────────
    // Try the hardware-accelerated MediaCodec decoder first on Android; fall
    // back to the software decoder on any other platform or if unavailable.
    const AVCodec *codec = avcodec_find_decoder_by_name("h264_mediacodec");
    if (!codec) {
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    }
    if (!codec) {
        qCritical() << "FrameManager: H.264 decoder not found";
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) return false;

    // Low-delay flag reduces decoder latency; safe because the server sends
    // intra-only (every frame is a keyframe, no B/P-frames).
    m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        qCritical() << "FrameManager: avcodec_open2 failed";
        return false;
    }

    // ── Decoded YUV frame ──────────────────────────────────────────────────────
    m_yuvFrame = av_frame_alloc();
    if (!m_yuvFrame) return false;

    // ── Output BGRA frame (used as QImage pixel buffer) ────────────────────────
    // Pixel format note:
    //   QImage::Format_ARGB32 stores each pixel as the 32-bit value 0xAARRGGBB.
    //   On little-endian ARM (all Android devices) the bytes in memory are
    //   B, G, R, A — which is exactly AV_PIX_FMT_BGRA.
    m_bgraFrame = av_frame_alloc();
    if (!m_bgraFrame) return false;

    m_bgraFrame->format = AV_PIX_FMT_BGRA;
    m_bgraFrame->width  = m_width;
    m_bgraFrame->height = m_height;
    if (av_frame_get_buffer(m_bgraFrame, 1) < 0) {
        qCritical() << "FrameManager: av_frame_get_buffer failed";
        return false;
    }

    // ── Packet ─────────────────────────────────────────────────────────────────
    m_packet = av_packet_alloc();
    if (!m_packet) return false;

    // ── Color-space conversion: YUV420P → BGRA ─────────────────────────────────
    m_swsCtx = sws_getContext(
        m_width, m_height, AV_PIX_FMT_YUV420P,
        m_width, m_height, AV_PIX_FMT_BGRA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsCtx) {
        qCritical() << "FrameManager: sws_getContext failed";
        return false;
    }

    return true;
}

void FrameManager::cleanupDecoder()
{
    if (m_swsCtx)    { sws_freeContext(m_swsCtx);         m_swsCtx    = nullptr; }
    if (m_packet)    { av_packet_free(&m_packet);          m_packet    = nullptr; }
    if (m_bgraFrame) { av_frame_free(&m_bgraFrame);        m_bgraFrame = nullptr; }
    if (m_yuvFrame)  { av_frame_free(&m_yuvFrame);         m_yuvFrame  = nullptr; }
    if (m_codecCtx)  { avcodec_free_context(&m_codecCtx); m_codecCtx  = nullptr; }
}

bool FrameManager::decodeFrame(const QByteArray &nalData)
{
    if (!m_codecCtx || !m_packet || !m_yuvFrame || !m_bgraFrame || !m_swsCtx) {
        return false;
    }

    // Allocate a new AVPacket buffer and copy the NAL data into it.
    av_packet_unref(m_packet);
    if (av_new_packet(m_packet, nalData.size()) < 0) {
        qWarning() << "FrameManager: av_new_packet failed";
        return false;
    }
    std::memcpy(m_packet->data, nalData.constData(), nalData.size());

    // Send the complete H.264 NAL unit to the decoder.
    int ret = avcodec_send_packet(m_codecCtx, m_packet);
    av_packet_unref(m_packet); // release the input buffer immediately
    if (ret < 0) {
        qWarning() << "FrameManager: avcodec_send_packet error" << ret;
        return false;
    }

    // Retrieve the decoded frame.
    ret = avcodec_receive_frame(m_codecCtx, m_yuvFrame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        // No frame available yet (shouldn't normally happen with intra-only
        // stream, but handle it gracefully).
        return false;
    }
    if (ret < 0) {
        qWarning() << "FrameManager: avcodec_receive_frame error" << ret;
        return false;
    }

    // Reallocate the BGRA frame if the decoded dimensions differ from what we
    // expected (e.g. server changed resolution).
    if (m_yuvFrame->width != m_bgraFrame->width ||
        m_yuvFrame->height != m_bgraFrame->height) {
        // Save dimensions before unreffing (av_frame_unref may zero them).
        const int newWidth  = m_yuvFrame->width;
        const int newHeight = m_yuvFrame->height;
        // Unref the decoded frame before tearing down the decoder so FFmpeg
        // can reclaim any internally referenced buffers.
        av_frame_unref(m_yuvFrame);
        setResolution(newWidth, newHeight);
        // setResolution called cleanupDecoder + initDecoder; bail out and
        // wait for the next frame with the updated decoder.
        return false;
    }

    // Convert YUV420P → BGRA.
    sws_scale(m_swsCtx,
              m_yuvFrame->data, m_yuvFrame->linesize, 0, m_yuvFrame->height,
              m_bgraFrame->data, m_bgraFrame->linesize);

    av_frame_unref(m_yuvFrame);

    // Wrap the BGRA pixels in a QImage.  The QImage does NOT own the pixels —
    // we deep-copy it before emitting so the signal receiver always holds a
    // stable, independent buffer.
    QImage img(m_bgraFrame->data[0],
               m_width, m_height,
               m_bgraFrame->linesize[0],
               QImage::Format_ARGB32);

    emit frameDecoded(img.copy());
    return true;
}
