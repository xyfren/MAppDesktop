#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <functional>
#include <vector>
#include <memory>
#include <set>
#include <mutex>

using namespace std;

class DataServer
{
public:
	DataServer(boost::asio::io_context& ioContext);
    ~DataServer();

    void run(uint16_t port);
    void send(const vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& targetEndpoint);

    void setMessageHandler(function<void(const vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& fromEndpoint)> messageHandler);
private:
    void handleUdpReceive();
    void handleSendResult(boost::system::error_code ec, size_t bytes_sent);

	boost::asio::io_context& m_ioContext;

    unique_ptr<boost::asio::ip::udp::socket> m_udpSocket;
    function<void(const vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& fromEndpoint)> m_messageHandler;

    array<uint8_t, 1500> m_receiveBuffer;
    boost::asio::ip::udp::endpoint m_remoteEndpoint;
};

