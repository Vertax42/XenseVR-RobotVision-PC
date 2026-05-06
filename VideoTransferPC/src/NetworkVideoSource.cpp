// Network video source implementation for receiving and decoding H264 streams
#include "NetworkVideoSource.h"
#include <iostream> // Replace QDebug

#ifdef _WIN32
#include <windows.h> // For OutputDebugStringA
#else
// Mock OutputDebugStringA for Linux
#define OutputDebugStringA(x) std::cerr << x << std::endl
#endif

NetworkVideoSource::NetworkVideoSource()
    : m_receiver(nullptr), m_decoder(nullptr) {}

NetworkVideoSource::~NetworkVideoSource() { stop(); }

void NetworkVideoSource::start(
    const std::string &ip, int port,
    std::function<void(const char *, int, int, int)> frameCallback) {
  std::cout << "NetworkVideoSource::start() called with ip: " << ip
            << " port: " << port << std::endl;
  // Create decoder
  m_decoder = new H264Decoder([frameCallback](const uint8_t *data, size_t size,
                                              int width, int height) {
    OutputDebugStringA((std::string("frameCallback decode output size: ") +
                        std::to_string(size))
                           .c_str());
    frameCallback(reinterpret_cast<const char *>(data), size, width, height);
  });

  // Create data receiver
  m_receiver = new CameraDataReceiver(ip, port);

  // Set data callback
  auto dataCallback = [this](const char *data, size_t length) {
    OutputDebugStringA((std::string("dataCallback decode input size: ") +
                        std::to_string(length))
                           .c_str());
    m_decoder->decode(reinterpret_cast<const uint8_t *>(data), length);
  };

  // Create thread using lambda expression
  m_networkThread = std::thread([this, dataCallback]() {
    try {
      std::cout << "Network thread: starting receiver" << std::endl;
      // Start receiver
      m_receiver->run(dataCallback);
      std::cout << "Network thread: receiver run completed" << std::endl;
    } catch (const std::exception &e) {
      std::cout << "Network thread exception: " << e.what() << std::endl;
    }
  });
}

void NetworkVideoSource::stop() {
  std::cout << "NetworkVideoSource::stop() called" << std::endl;
  if (m_receiver) {
    m_receiver->stop();
    std::cout << "NetworkVideoSource: receiver stopped" << std::endl;
  }

  if (m_networkThread.joinable()) {
    std::cout << "NetworkVideoSource: waiting for network thread to join"
              << std::endl;
    m_networkThread.join();
  }

  if (m_receiver) {
    delete m_receiver;
    m_receiver = nullptr;
    std::cout << "NetworkVideoSource: receiver deleted" << std::endl;
  }

  if (m_decoder) {
    delete m_decoder;
    m_decoder = nullptr;
    std::cout << "NetworkVideoSource: decoder deleted" << std::endl;
  }
  std::cout << "NetworkVideoSource::stop() completed" << std::endl;
}
