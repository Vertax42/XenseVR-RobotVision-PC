//H264 encoder class for encoding video frames to H264 format using FFmpeg
#pragma once

#include <cstdint>
#include <functional>

// Forward declarations of required FFmpeg structures
struct AVCodecContext;
struct AVFrame;
struct AVPacket;

// Define callback function type
using AVpacketWriteCallback = std::function<void(const uint8_t* data, size_t size)>;

class H264Encoder {
private:
    AVCodecContext* m_encCtx;
    AVFrame* m_frame;
    AVPacket* m_pkt;
    AVpacketWriteCallback m_writeCallback;
    int64_t m_ptsCounter;

public:
    H264Encoder(int width, int height, AVpacketWriteCallback writeCallback, int fps, int64_t bitrate = 4000000);

    void encodeFrame(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                     size_t y_size, size_t u_size, size_t v_size);

    void finalize();

    ~H264Encoder();
};
