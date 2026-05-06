//Network data sender implementation for camera streaming using ASIO
#include "CameraDataSender.h"

CameraDataSender::CameraDataSender(const std::string& host, int port, const std::string& bindIP)
    : m_host(host), m_port(port), m_bindIP(bindIP), m_socket(m_ioContext) {
}

void CameraDataSender::startConnect(std::function<void(CameraDataSender&)> runCallback) {
    std::cout << "Starting connect to " << m_host << ":" << m_port << std::endl;

    // Check and open socket if not already opened
    if (!m_socket.is_open()) {
        std::cout << "Socket is not open, opening socket with TCP v4" << std::endl;
        try {
            m_socket.open(asio::ip::tcp::v4());
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to open socket: " << e.what() << std::endl;
            throw;
        }
    }

    // If binding IP is specified, perform binding operation, otherwise use default network interface
    if (!m_bindIP.empty()) {
        try {
            auto local_endpoint = asio::ip::tcp::endpoint(asio::ip::address::from_string(m_bindIP), 0);
            m_socket.bind(local_endpoint);
            std::cout << "Socket bound to " << local_endpoint.address().to_string() << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Socket bind error: " << e.what() << std::endl;
            throw;
        }
    }
    else {
        std::cout << "Using default network interface." << std::endl;
    }

    asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(m_host), m_port);
    m_socket.async_connect(endpoint, [runCallback, this](std::error_code ec) {
        if (ec) {
            std::cout << "Connection failed: " << ec.message() << std::endl;
        }
        else {
            std::cout << "Connection succeeded" << std::endl;
        }
        runCallback(*this);
        });
}

void CameraDataSender::sendData(const char* data, uint32_t dataLength) {
    // Check if connection is successful
    if (!m_socket.is_open()) {
        // Now, we throw an exception here because the connection is not active.
        throw SendDataException("Socket is not connected. Cannot send data.");
    }

    asio::error_code error;
    size_t total_bytes_sent = 0;

    // Loop until all data is sent
    while (total_bytes_sent < dataLength) {
        size_t bytes_sent = m_socket.send(asio::buffer(data + total_bytes_sent, dataLength - total_bytes_sent), 0, error);
        if (error) {
            // Throw an exception on send error
            throw SendDataException("Send error: " + error.message());
        }
        total_bytes_sent += bytes_sent;
    }
}


void CameraDataSender::disconnect() {
    m_socket.close();
    std::cout << "Client disconnected" << std::endl;
}

void CameraDataSender::runIOContext() {
    std::cout << "Starting io_context.run()" << std::endl;
    m_ioContext.run();
    std::cout << "io_context.run() completed" << std::endl;
}