//Camera data sender class for transmitting video data over TCP network
#ifndef CAMERATCPSENDER_H
#define CAMERATCPSENDER_H

#include <asio.hpp>
#include <iostream>
#include <functional>

class SendDataException : public std::runtime_error {
public:
    explicit SendDataException(const std::string& message)
        : std::runtime_error(message) {}
};

class CameraDataSender {
public:
    CameraDataSender(const std::string& host, int port, const std::string& bindIP = "");
    void startConnect(std::function<void(CameraDataSender&)> runCallback);
    void sendData(const char* data, uint32_t dataLength);
    void disconnect();
    void runIOContext();

private:
    std::string m_host;
    int m_port;
    std::string m_bindIP;
    asio::io_context m_ioContext;
    asio::ip::tcp::socket m_socket;
};

#endif // CAMERATCPSENDER_H
