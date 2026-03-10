# Android Qt Client — FrameManager

`FrameManager` is the H.264 frame-receiver/decoder for the Android Qt client.
It is a drop-in replacement for any previous JPEG-based frame handler and
matches the server-side `FFmpegCoder` that was added in the *optimize
frame-sending* commit.

---

## How it works

```
UDP datagram  ──►  processPacket()
                       │
                 FPacket parse & validate
                       │
                 reassembly buffer  (QMap<frameId, PartialFrame>)
                       │
              all parts received?
                  YES  │
                 decodeFrame()
                       │
            avcodec_send_packet / receive_frame
                       │
            sws_scale  YUV420P → BGRA
                       │
            emit frameDecoded(QImage)  ──►  display widget
```

Every frame sent by the server is an IDR (intra-only) frame, so the decoder
never needs prior state to decode the current frame.  Stale incomplete frames
(caused by packet loss) are automatically discarded when newer frames arrive.

---

## Integration

### 1. FFmpeg libraries

The class requires **libavcodec**, **libavutil**, and **libswscale** (FFmpeg ≥ 4.0).

#### Via vcpkg (desktop / Windows testing)
```
vcpkg install ffmpeg[x264]:x64-windows
```

#### Android pre-built binaries
Several options are available:
- **ffmpeg-kit** (recommended): <https://github.com/arthenica/ffmpeg-kit>
  — provides a ready-made AAR with all required `.so` files.
- Build FFmpeg from source targeting Android NDK (see the [official Android
  build guide](https://trac.ffmpeg.org/wiki/CompilationGuide/Android)).

Place the `.so` files (`libavcodec.so`, `libavutil.so`, `libswscale.so`) in
your project's `jniLibs/arm64-v8a/` (and other ABI dirs as needed).

### 2. qmake (`.pro` file)

```qmake
# Android: point at the pre-built FFmpeg tree
android {
    FFMPEG_DIR = $$PWD/libs/ffmpeg-android

    INCLUDEPATH += $$FFMPEG_DIR/include
    LIBS += -L$$FFMPEG_DIR/lib/arm64-v8a \
            -lavcodec -lavutil -lswscale
}

# Desktop (for testing)
linux|win32 {
    INCLUDEPATH += /path/to/ffmpeg/include
    LIBS += -lavcodec -lavutil -lswscale
}

SOURCES += FrameManager.cpp
HEADERS += FrameManager.h
```

### 3. CMake (`CMakeLists.txt`)

```cmake
find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil swscale)

target_include_directories(MyApp PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(MyApp PRIVATE
    Qt6::Core Qt6::Gui
    ${FFMPEG_LIBRARIES}
)
target_sources(MyApp PRIVATE FrameManager.cpp FrameManager.h)
```

---

## Usage example

```cpp
// In your connection handler, once you know the monitor resolution:
FrameManager *fm = new FrameManager(width, height, this);

connect(fm, &FrameManager::frameDecoded,
        myVideoWidget, &VideoWidget::setFrame);   // or QLabel::setPixmap, etc.

// In your QUdpSocket readyRead slot:
connect(udpSocket, &QUdpSocket::readyRead, this, [=]() {
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram(udpSocket->pendingDatagramSize(), Qt::Uninitialized);
        udpSocket->readDatagram(datagram.data(), datagram.size());
        fm->processPacket(datagram);
    }
});
```

---

## Hardware acceleration on Android

`FrameManager::initDecoder()` first tries `h264_mediacodec` (Android MediaCodec
hardware decoder) and falls back to the software `h264` decoder automatically.
No additional configuration is needed.

---

## Wire protocol reference

The server sends one or more `FPacket` UDP datagrams per frame.

| Field        | Type      | Offset | Description                                |
|--------------|-----------|--------|--------------------------------------------|
| `type`       | uint16 LE |  0     | `310` = H.264 fragment (`FPACKET_TYPE_H264`) |
| `frameId`    | uint64 LE |  2     | Monotonically increasing frame counter     |
| `totalParts` | uint16 LE | 10     | Total number of fragments for this frame   |
| `partId`     | uint16 LE | 12     | Zero-based index of this fragment          |
| `partOffset` | uint32 LE | 14     | Byte offset of `partData` in the full frame|
| `partSize`   | uint16 LE | 18     | Bytes of payload in this datagram (≤ 1300) |
| `partData`   | uint8[]   | 20     | H.264 NAL unit fragment                    |

Maximum datagram size: 20 (header) + 1300 (payload) + 8 (UDP) + 20 (IP) = 1348 bytes,
which fits in a standard 1500-byte Ethernet MTU without IP fragmentation.
