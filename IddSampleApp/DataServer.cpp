#include "DataServer.h"
#include <iostream>

DataServer::DataServer(boost::asio::io_context& ioContext):
    m_ioContext(ioContext)
{
    m_receiveBuffer = {0};
}

DataServer::~DataServer() {
    if (m_udpSocket) {
        m_udpSocket->close();
    }
}

void DataServer::run(uint16_t port) {
    try {
        m_udpSocket = make_unique<udp::socket>(m_ioContext);
        udp::endpoint endpoint(udp::v4(), port);
        m_udpSocket->open(endpoint.protocol());
        m_udpSocket->set_option(boost::asio::socket_base::broadcast(true));
        boost::asio::socket_base::send_buffer_size bufSize(1024 * 1024 * 4); // 1 MB
        m_udpSocket->set_option(bufSize);
        m_udpSocket->bind(endpoint);

        cout << "Data server started on port " << port << endl;
        handleUdpReceive();
    }
    catch (const exception& ex) {
        cerr << "Error running DataServer: " << ex.what() << endl;
    }
}

void DataServer::handleUdpReceive() {
    if (!m_udpSocket) return;

    m_udpSocket->async_receive_from(
        boost::asio::buffer(m_receiveBuffer), m_remoteEndpoint,
        [this](boost::system::error_code ec, size_t bytes_recvd) {
            if (!ec && bytes_recvd > 0 && m_messageHandler) {
                m_messageHandler(
                    vector<uint8_t>(m_receiveBuffer.begin(),
                        m_receiveBuffer.begin() + bytes_recvd),
                        m_remoteEndpoint
                    );
            }

            handleUdpReceive();
        });
}

void DataServer::send(const vector<uint8_t>& data, const udp::endpoint& targetEndpoint) {
    if (!m_udpSocket) return;
    if (!m_udpSocket->is_open()) return;

    auto sendBuffer = make_shared<vector<uint8_t>>(data);

    m_udpSocket->async_send_to(
        boost::asio::buffer(*sendBuffer),
        targetEndpoint,
        [this, sendBuffer](boost::system::error_code ec, size_t bytes_sent) {
            handleSendResult(ec, bytes_sent);
        }
    );
}

void DataServer::sendSPackets(std::span<const SPacket> packets,
                              const udp::endpoint& targetEndpoint)
{
    if (!m_udpSocket || !m_udpSocket->is_open()) return;
    if (packets.empty()) return;

    // Drop the frame if the previous one has not finished sending yet.
    // This provides back-pressure without unbounded kernel-buffer growth.
    const int MAX_IN_FLIGHT = static_cast<int>(packets.size() * 2);
    if (m_packetsInFlight > MAX_IN_FLIGHT) {
        printf("drop\n");
        return;
    }

    // Single allocation for the entire frame batch — all async send lambdas
    // share the same shared_ptr so the data stays alive until every send
    // completes, without allocating per-packet.
    auto batch = std::make_shared<std::vector<SPacket>>(packets.begin(), packets.end());
    m_packetsInFlight += static_cast<int>(batch->size());

    for (size_t i = 0; i < batch->size(); ++i) {
        const SPacket* pkt = &(*batch)[i];
        m_udpSocket->async_send_to(
            boost::asio::buffer(pkt->rawData(),
                                SPACKET_HEADER_SIZE + pkt->dataSize),
            targetEndpoint,
            [this, batch](boost::system::error_code ec, size_t bytes_sent) {
                --m_packetsInFlight;
                if (ec) {
                    cerr << "DataServer: send error: " << ec.message() << "\n";
                } else {
                    ++m_packetsSent;
                    m_bytesSent += bytes_sent;
                }
            }
        );
    }
    ++m_framesSent;
}

void DataServer::handleSendResult(boost::system::error_code ec, size_t bytes_sent) {
	cout << "Sent " << bytes_sent << " bytes" << endl;
    if (ec) {
        cerr << "Error sending UDP data: " << ec << endl;

        if (ec == boost::asio::error::connection_refused) {
            cerr << "Connection refused by remote host" << endl;
        }
    }
}

void DataServer::setMessageHandler(function<void(const vector<uint8_t>& data, const udp::endpoint& fromEndpoint)> messageHandler) {
    m_messageHandler = messageHandler;
}

std::optional<boost::asio::ip::udp::endpoint> DataServer::getLocalUdpEndpoint() {
    if (!m_udpSocket) {
        return std::nullopt;
    }

    if (!m_udpSocket->is_open()) {
        return std::nullopt;
    }

    boost::system::error_code ec;

    auto endpoint = m_udpSocket->local_endpoint(ec);

    if (ec) {
        return std::nullopt;
    }

    return endpoint;
}
