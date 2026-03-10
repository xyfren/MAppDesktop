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
        boost::asio::socket_base::send_buffer_size option(1024 * 1024); // 1 МБ
        m_udpSocket->set_option(option);
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

void DataServer::send(const vector<uint8_t>& data, const udp::endpoint& targetEndpoint) {
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

void DataServer::sendFPacket(shared_ptr<FPacket> packet, const udp::endpoint& targetEndpoint) {
    if (!m_udpSocket) {
        std::cerr << "❌ UDP socket not initialized" << std::endl;
        return;
    }

    m_udpSocket->async_send_to(
        boost::asio::buffer(packet->rawData(), FPACKET_HEADER_SIZE + packet->partSize),
        targetEndpoint,
        [this, packet](boost::system::error_code ec, size_t bytes_sent) {
            // packet жив благодаря захвату по значению
            m_packetsInFlight--;
            if (ec) {
                std::cerr << "❌ Send error: " << ec.message() << std::endl;
            }
            else {
                m_packetsSent++; 
                m_bytesSent += bytes_sent;

                // Можно добавить отладку для последнего пакета кадра
                if (packet->partId == packet->totalParts - 1) {
                    //std::cout << "📦 Кадр #" << packet->frameId
                    //    << " полностью отправлен. Пакетов: " << packet->totalParts
                    //    << " Статистика: всего пакетов=" << m_packetsSent << std::endl;
                }
            }
        }
    );
}

void DataServer::sendFrame(span<uint8_t>& frameData, const udp::endpoint& targetEndpoint) {

    // Drop frame if the previous one is still being sent to avoid growing the
    // kernel send-buffer without bound (back-pressure / frame-dropping policy).
    const int MAX_PACKETS_IN_FLIGHT = 50;
    if (m_packetsInFlight > MAX_PACKETS_IN_FLIGHT) {
        return;
    }

    uint32_t totalPackets = (frameData.size() + FPACKET_MAX_FRAME_SIZE - 1) / FPACKET_MAX_FRAME_SIZE;

    m_packetsInFlight = totalPackets;

    static uint32_t frameNumber = 0;
    
    // Создаём и отправляем каждый пакет
    for (uint32_t packetId = 0; packetId < totalPackets; ++packetId) {
        // Вычисляем смещение и размер для этого пакета
        size_t offset = packetId * FPACKET_MAX_FRAME_SIZE;
        size_t remainingSize = frameData.size() - offset;
        uint16_t packetDataSize = std::min((size_t )FPACKET_MAX_FRAME_SIZE, remainingSize);

        // Создаём пакет (единственное копирование данных!)
        auto packet = std::make_shared<FPacket>();
        packet->type = FPACKET_TYPE_H264;
        packet->frameId = frameNumber;

        packet->totalParts = totalPackets;
        packet->partId = packetId;
        packet->partOffset = offset;
        packet->partSize = packetDataSize;
        //std::cout << "packet->partId 0" << packet->partId << endl;
        //std::cout << "packet->partOffset " << packet->partOffset << endl;
        //std::cout << "packet->partSize " << packet->partSize << endl;


        // Копируем данные кадра в пакет
        std::memcpy(packet->partData, frameData.data() + offset, packetDataSize);
        sendFPacket(packet, targetEndpoint);
    }
    frameNumber++;
}

void DataServer::handleSendResult(boost::system::error_code ec, size_t bytes_sent) {
    if (ec) {
        cerr << "Ошибка отправки UDP данных: " << ec.message() << endl;

        if (ec == boost::asio::error::connection_refused) {
            cerr << "Соединение отклонено удаленным хостом" << endl;
        }
    }
}

void DataServer::setMessageHandler(function<void(const vector<uint8_t>& data, const udp::endpoint& fromEndpoint)> messageHandler) {
    m_messageHandler = messageHandler;
}