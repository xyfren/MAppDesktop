#ifndef FRAMEMANAGER_H
#define FRAMEMANAGER_H

#include <QObject>
#include <QByteArray>
#include <QImage>
#include <QMap>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// ─── Wire-protocol constants (must match server FPacket.h) ───────────────────

// Packet type sent by the server for H.264-encoded frame fragments.
static constexpr quint16 FPACKET_TYPE_H264 = 310;

// Size of the packed FPacket header on the wire (bytes).
static constexpr int FPACKET_HEADER_SIZE = 20;

// Maximum payload bytes per UDP datagram (server splits frames at this boundary).
static constexpr int FPACKET_MAX_FRAME_SIZE = 1300;

// Binary-compatible mirror of the server's FPacket header (packed, LE).
// Layout:
//   offset  0 : type       (uint16)
//   offset  2 : frameId    (uint64)
//   offset 10 : totalParts (uint16)
//   offset 12 : partId     (uint16)
//   offset 14 : partOffset (uint32)
//   offset 18 : partSize   (uint16)
//   offset 20 : partData[partSize]
#pragma pack(push, 1)
struct FPacketHeader {
    quint16 type;
    quint64 frameId;
    quint16 totalParts;
    quint16 partId;
    quint32 partOffset;
    quint16 partSize;
};
#pragma pack(pop)

// ─── FrameManager ─────────────────────────────────────────────────────────────

/**
 * @brief Reassembles fragmented H.264 UDP frames and decodes them to QImage.
 *
 * Usage:
 *   1. Create an instance once you know the video dimensions (from the server's
 *      auth response / APacket).
 *   2. Connect the frameDecoded() signal to your display widget.
 *   3. Call processPacket() for every UDP datagram received from the server.
 *
 * Thread safety: processPacket() must be called from a single thread (e.g. the
 * Qt network thread or the main thread).  The frameDecoded() signal is emitted
 * synchronously in that same thread.
 *
 * Dependencies: libavcodec, libavutil, libswscale (FFmpeg ≥ 4.0).
 * On Android these are usually provided by a pre-built FFmpeg AAR or compiled
 * from source; see README.md for details.
 */
class FrameManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @param width   Frame width in pixels (must match the server monitor config).
     * @param height  Frame height in pixels.
     * @param parent  Optional Qt parent object.
     */
    explicit FrameManager(int width, int height, QObject *parent = nullptr);
    ~FrameManager() override;

    /**
     * @brief Feed one raw UDP datagram to the reassembler/decoder.
     *
     * When the last fragment of a frame arrives, the assembled H.264 NAL unit
     * buffer is decoded and frameDecoded() is emitted.
     *
     * Fragments that belong to frames older than the current frame window are
     * silently discarded to avoid unbounded memory growth on packet loss.
     */
    void processPacket(const QByteArray &data);

    /**
     * @brief Resize the decoder for a new resolution.
     *
     * Tears down and re-initialises the FFmpeg decoder and swscale context.
     * Any in-flight partial frames are discarded.
     */
    void setResolution(int width, int height);

signals:
    /**
     * Emitted once per fully decoded frame.
     * The QImage is in QImage::Format_ARGB32 and is a deep copy — safe to pass
     * across threads or store beyond the slot call.
     */
    void frameDecoded(const QImage &image);

private:
    // ── Reassembly state ──────────────────────────────────────────────────────

    struct PartialFrame {
        QByteArray buffer;         ///< Assembled H.264 data (sized to partOffset+partSize)
        quint16    totalParts = 0; ///< Expected number of fragments
        quint16    receivedParts = 0;
    };

    // Map from frameId to partial-frame state.
    QMap<quint64, PartialFrame> m_partialFrames;

    /// Drop frames with IDs more than kStaleWindowSize behind currentFrameId.
    static constexpr quint64 kStaleWindowSize = 4;
    void dropStaleFrames(quint64 currentFrameId);

    // ── FFmpeg decoder ────────────────────────────────────────────────────────

    int m_width  = 0;
    int m_height = 0;

    AVCodecContext *m_codecCtx  = nullptr;
    AVFrame        *m_yuvFrame  = nullptr; ///< Decoded YUV420P frame
    AVFrame        *m_bgraFrame = nullptr; ///< Converted BGRA frame (QImage buffer)
    AVPacket       *m_packet    = nullptr;
    SwsContext     *m_swsCtx    = nullptr;

    bool initDecoder();
    void cleanupDecoder();

    /// Decode one complete H.264 NAL unit buffer and emit frameDecoded().
    bool decodeFrame(const QByteArray &nalData);
};

#endif // FRAMEMANAGER_H
