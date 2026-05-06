//Network video source class for receiving and decoding video streams over network
#ifndef NETWORKVIDEOSOURCE_H
#define NETWORKVIDEOSOURCE_H

#include "CameraDataReceiver.h"
#include "H264Decoder.h"
#include <functional>
#include <thread>
#include <atomic>

class NetworkVideoSource
{
public:
    explicit NetworkVideoSource();
    ~NetworkVideoSource();

    void start(const std::string& ip, int port, std::function<void(const char*, int, int, int)> frameCallback);
    void stop();

private:
    CameraDataReceiver* m_receiver;
    H264Decoder* m_decoder;
    std::thread m_networkThread;
};

#endif // NETWORKVIDEOSOURCE_H
