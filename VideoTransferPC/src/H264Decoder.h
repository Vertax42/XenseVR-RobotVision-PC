//H264 decoder class for decoding H264 video streams using FFmpeg
#pragma once

#include <cstdint>
#include <functional>

// Forward declarations of required FFmpeg structures
struct AVCodecContext;
struct AVFrame;
struct AVPacket;

// Define callback function type
using DecodeCallback = std::function<void(const uint8_t* data, size_t size, int width, int height)>;

class H264Decoder {
private:
    AVCodecContext* m_decCtx;
    AVFrame* m_frame;
    AVPacket* m_pkt;
    bool m_initialized;
    DecodeCallback m_callback;

    // Add private method for resource cleanup
    void cleanup();

public:
    H264Decoder(DecodeCallback callback);
    ~H264Decoder();

    void decode(const uint8_t* data, size_t size);
};
