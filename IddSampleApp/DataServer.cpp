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
        m_udpSocket = make_unique<boost::asio::ip::udp::socket>(m_ioContext);
        boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::udp::v4(), port);
        m_udpSocket->open(endpoint.protocol());
        m_udpSocket->bind(endpoint);

        cout << "Дата сервер запущен на порту " << port << endl;
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

void DataServer::send(const vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& targetEndpoint) {
    if (!m_udpSocket) return;

    // Создаем shared_ptr для продления времени жизни данных
    auto sendBuffer = make_shared<vector<uint8_t>>(data);

    m_udpSocket->async_send_to(
        boost::asio::buffer(*sendBuffer),
        targetEndpoint,
        [this, sendBuffer](boost::system::error_code ec, size_t bytes_sent) {
            handleSendResult(ec, bytes_sent);
        }
    );
}

void DataServer::handleSendResult(boost::system::error_code ec, size_t bytes_sent) {
    if (ec) {
        cerr << "Ошибка отправки UDP данных: " << ec.message() << endl;

        // Можно добавить дополнительную обработку ошибок, например:
        if (ec == boost::asio::error::connection_refused) {
            cerr << "Соединение отклонено удаленным хостом" << endl;
        }
    }
    else {
        cout << "Отправлено " << bytes_sent << " байт" << endl;
    }
}

void DataServer::setMessageHandler(function<void(const vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& fromEndpoint)> messageHandler) {
    m_messageHandler = messageHandler;
}