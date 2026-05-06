//Camera capture class for capturing video frames using FFmpeg
#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <atomic>

// Forward declarations of required FFmpeg structures
struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;
struct AVFrame;
struct AVPacket;
struct AVDictionary;

// Callback function type definition
using FrameCallback = std::function<void(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                                       size_t y_size, size_t u_size, size_t v_size,
                                       int width, int height, int frame_index)>;

class CameraCapture {
private:
    // Configuration parameters
    std::string m_deviceName;
    std::string m_videoSize;
    std::string m_framerate;
    std::string m_codecName;
    int m_targetFrameCount;
    std::atomic<bool> m_shouldStop{false};

    // Helper methods
    static void custom_av_log(void* ptr, int level, const char* fmt, va_list vl);
    static void defaultFrameCallback(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                                   size_t y_size, size_t u_size, size_t v_size,
                                   int width, int height, int frame_index);

public:
    CameraCapture(const char* device, const char* size, const char* rate,
                 const char* codec, int frames);

    int run(FrameCallback callback = defaultFrameCallback);

    void stopCapture();
    ~CameraCapture() = default;
};
