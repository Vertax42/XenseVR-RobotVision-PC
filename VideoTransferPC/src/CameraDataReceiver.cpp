//Network data receiver implementation for camera streaming using ASIO
#include "CameraDataReceiver.h"

CameraDataReceiver::CameraDataReceiver(const std::string& ip, int port)
    : m_ip(ip), m_port(port), m_acceptor(m_io_context, asio::ip::tcp::endpoint(asio::ip::make_address(m_ip), m_port)), m_should_exit(false) {
}

void CameraDataReceiver::run(DataCallback callback) {
    m_dataCallback = callback;
    std::cout << "CameraDataReceiver::run() started" << std::endl;

    // Start accepting connections
    acceptConnections();

    // Start IO context event loop
    m_io_context.run();
    std::cout << "CameraDataReceiver: io_context run completed" << std::endl;
}

void CameraDataReceiver::stop() {
    std::cout << "CameraDataReceiver::stop() called, setting exit flag" << std::endl;
    m_should_exit = true;

    try {
        if (m_acceptor.is_open()) {
            asio::error_code ec;
            // First cancel all pending async operations
            m_acceptor.cancel(ec);
            m_acceptor.close(ec);
            if (ec) {
                std::cerr << "Error closing acceptor: " << ec.message() << std::endl;
            }
            std::cout << "CameraDataReceiver: acceptor closed" << std::endl;
        }

        // Stop io_context, this will cause all unfinished async operations to be cancelled
        m_io_context.stop();
        std::cout << "CameraDataReceiver: io_context stopped" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during stop: " << e.what() << std::endl;
    }
}

void CameraDataReceiver::acceptConnections() {
    if (m_should_exit) {
        std::cout << "CameraDataReceiver::acceptConnections() exiting due to stop flag" <<std::endl;
        return;
    }

    auto socket = std::make_shared<asio::ip::tcp::socket>(m_io_context);

    m_acceptor.async_accept(*socket, [this, socket](const asio::error_code& error) {
        if (!error) {
            std::cout << "Client connected" << std::endl;
            handleClient(socket);
        } else {
                std::cout << "Accept error: " << error.message() << std::endl;
        }

        if (!m_should_exit) {
            acceptConnections();
        } else {
            std::cout << "CameraDataReceiver: stopping accept loop due to stop flag" << std::endl;
        }
    });
}

void CameraDataReceiver::handleClient(std::shared_ptr<asio::ip::tcp::socket> socket) {
    while (socket->is_open() && !m_should_exit) {
        try {
            // First, receive the 4-byte big-endian length header
            std::array<uint8_t, 4> length_bytes;
            asio::error_code error;

            size_t header_bytes_read = asio::read(*socket, asio::buffer(length_bytes), error);

            if (error) {
                if (error == asio::error::eof || error == asio::error::connection_reset) {
                    std::cout << "Client disconnected." << std::endl;
                }
                else {
                    std::cerr << "Error reading length header: " << error.message() << std::endl;
                }
                break;
            }

            if (header_bytes_read != sizeof(length_bytes)) {
                std::cerr << "Incomplete length header received. Expected " << sizeof(length_bytes) << " bytes, got " << header_bytes_read << std::endl;
                break;
            }

            // Reconstruct dataLength from big-endian bytes
            uint32_t dataLength = (static_cast<uint32_t>(length_bytes[0]) << 24) |
                (static_cast<uint32_t>(length_bytes[1]) << 16) |
                (static_cast<uint32_t>(length_bytes[2]) << 8) |
                static_cast<uint32_t>(length_bytes[3]);

            if (dataLength == 0) {
                // Handle empty data or keep-alive if applicable
                std::cout << "Received 0-length packet. Continuing..." << std::endl;
                continue;
            }

            // Create buffer based on received length
            std::vector<char> buffer(dataLength);

            // Read the data payload of specified length
            size_t bytes_read = asio::read(*socket, asio::buffer(buffer), error);

            if (error) {
                std::cerr << "Error reading data payload: " << error.message() << std::endl;
                break;
            }

            if (bytes_read != dataLength) {
                std::cerr << "Incomplete data payload received. Expected " << dataLength << " bytes, got " << bytes_read << std::endl;
                // Depending on your protocol, you might need to handle this as a fatal error or try to recover
                break;
            }

            // Callback with the received data and its actual size
            // Note: m_dataCallback expects the actual data, not including the length prefix.
            m_dataCallback(buffer.data(), bytes_read);
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in handleClient: " << e.what() << std::endl;
            break; // Exit the loop on exception
        }
    }
    std::cout << "Client handler stopped." << std::endl;
    // Socket will be automatically closed when shared_ptr is destructed
}