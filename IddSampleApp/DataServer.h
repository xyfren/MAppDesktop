#pragma once

#define BOOST_ASIO_DISABLE_WARNING_PUSH
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

#include <functional>
#include <vector>
#include <memory>
#include <set>
#include <mutex>
#include <span>

#include "DPacket.h"
#include "FPacket.h"

using namespace std;

using namespace boost::asio::ip;

class DataServer
{
public:
	DataServer(boost::asio::io_context& ioContext);
    ~DataServer();

    void run(uint16_t port);
    void send(const vector<uint8_t>& data, const udp::endpoint& targetEndpoint);
    void sendFPacket(shared_ptr<FPacket> packet, const udp::endpoint& targetEndpoint);

    void sendFrame(span<uint8_t>& frameData, const udp::endpoint& targetEndpoint);

    void setMessageHandler(function<void(const vector<uint8_t>& data, const udp::endpoint& fromEndpoint)> messageHandler);
private:
    void handleUdpReceive();
    void handleSendResult(boost::system::error_code ec, size_t bytes_sent);

	boost::asio::io_context& m_ioContext;

    unique_ptr<udp::socket> m_udpSocket;
    function<void(const vector<uint8_t>& data, const udp::endpoint& fromEndpoint)> m_messageHandler;

    array<uint8_t, 1500> m_receiveBuffer;
    udp::endpoint m_remoteEndpoint;

    std::atomic<size_t> m_framesSent{ 0 };
    std::atomic<size_t> m_packetsSent{ 0 };
    std::atomic<size_t> m_bytesSent{ 0 };
};

