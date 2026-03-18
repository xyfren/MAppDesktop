#pragma once

#define BOOST_ASIO_DISABLE_WARNING_PUSH
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/locale.hpp>

#include <coroutine>
#include <functional>
#include <vector>
#include <memory>
#include <set>
#include <mutex>
#include <span>

#include "SPacket.h"

using boost::asio::ip::tcp;

using namespace std;

class ConnectionServer
{
public:
    ConnectionServer(boost::asio::io_context &ioContext);
    ~ConnectionServer();

    void run(uint16_t port);
    void stop();

    void send(shared_ptr<vector<uint8_t>> pData, shared_ptr<tcp::socket> socket);
    void broadcastData(shared_ptr<vector<uint8_t>> pData);
    void broadcastData(shared_ptr<vector<uint8_t>> pData, std::function<bool(const std::shared_ptr<tcp::socket>&)> filter);

    void sendSPackets(std::span<const SPacket> packets, shared_ptr<tcp::socket> socket);

    set<shared_ptr<tcp::socket>> getConnections();

    // Обработчики событий (переопределяются пользователем)
    void setOpenHandler(function<void(shared_ptr<tcp::socket> socket)> openHandler);
    void setMessageHandler(function<void(const vector<uint8_t>& data, shared_ptr<tcp::socket> socket)> messageHandler);
    void setClosehandler(function<void(shared_ptr<tcp::socket> socket)> closeHandler);

private:
    boost::asio::awaitable<void> startAcceptor(uint16_t port); // корутина принимающая соедининия
    boost::asio::awaitable<void> clientHandler(shared_ptr<tcp::socket> socket); // корутины клиентов
    void addConnection(shared_ptr<tcp::socket> socket);
    void removeConnection(shared_ptr<tcp::socket> socket);

    function<void(const vector<uint8_t>& data, shared_ptr<tcp::socket> socket)> messageHandler;
    function<void(shared_ptr<tcp::socket> socket)> openHandler;
    function<void(shared_ptr<tcp::socket> socket)> closeHandler;

    boost::asio::io_context &m_ioContext;
    tcp::acceptor m_acceptor;

    set<shared_ptr<tcp::socket>> m_connections;
    mutex m_connectionsMutex;

    static const size_t BUFFER_SIZE = 65536;
    //array<uint8_t, BUFFER_SIZE> m_buffer;
    std::atomic<size_t> m_framesSent{ 0 };
    std::atomic<size_t> m_packetsSent{ 0 };
    std::atomic<size_t> m_bytesSent{ 0 };

    std::atomic<int> m_packetsInFlight{ 0 };

};

