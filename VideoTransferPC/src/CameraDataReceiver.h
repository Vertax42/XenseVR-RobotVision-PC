//Camera data receiver class for receiving video data over TCP network
#ifndef CAMERATCPRECEIVER_H
#define CAMERATCPRECEIVER_H

#include <asio.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <functional>
#include <atomic>

class CameraDataReceiver {
public:
    // Define callback function type, receive data buffer and data length
    using DataCallback = std::function<void(const char*, size_t)>;

    CameraDataReceiver(const std::string& ip, int port);
    void run(DataCallback callback);
    void stop();

private:
    void acceptConnections();
    void handleClient(std::shared_ptr<asio::ip::tcp::socket> socket);

    std::string m_ip;
    int m_port;
    asio::io_context m_io_context;
    asio::ip::tcp::acceptor m_acceptor;
    std::atomic_bool m_should_exit;
    DataCallback m_dataCallback;
};

#endif // CAMERATCPRECEIVER_H
