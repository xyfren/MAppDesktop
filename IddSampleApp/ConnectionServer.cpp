// ConnectionServer.cpp
#include "ConnectionServer.h"
#include <iostream>
#include <algorithm>

ConnectionServer::ConnectionServer(boost::asio::io_context &ioContext): 
    m_ioContext(ioContext),
    m_acceptor(ioContext)
{
    openHandler = nullptr;
    messageHandler = nullptr;
    closeHandler = nullptr;
}

ConnectionServer::~ConnectionServer() {
    stop();
}

void ConnectionServer::run(uint16_t port) {
    // Запускаем acceptor в виде корутины
    boost::asio::co_spawn(m_ioContext,
        startAcceptor(port),
        boost::asio::detached);

    cout << "Сервер запущен на порту: " << port << endl;
}

boost::asio::awaitable<void> ConnectionServer::startAcceptor(uint16_t port) {
    try {
        auto executor = co_await boost::asio::this_coro::executor;

        tcp::endpoint endpoint(tcp::v4(), port);
        m_acceptor.open(endpoint.protocol());
        m_acceptor.set_option(tcp::acceptor::reuse_address(true));
        m_acceptor.bind(endpoint);
        m_acceptor.listen();

        cout << "Acceptor готов на порту " << port << endl;

        while (true) {
            shared_ptr<tcp::socket> socket = make_shared<tcp::socket>(co_await m_acceptor.async_accept(boost::asio::use_awaitable));

            // Запускаем обработку клиента в новой корутине
            boost::asio::co_spawn(executor,
                clientHandler(socket),
                boost::asio::detached);
        }
    }
    catch (const exception& ex) {
        cerr << "Ошибка в acceptor корутине: " << ex.what() << endl;
    }
}

boost::asio::awaitable<void> ConnectionServer::clientHandler(shared_ptr<tcp::socket> socket) {
    try {
        addConnection(socket);

        if (openHandler) {
            openHandler(socket);
        }

        // Основной цикл чтения
        while (socket->is_open()) {
            vector<uint8_t> buffer(BUFFER_SIZE);

            size_t bytes_read = co_await socket->async_read_some(
                boost::asio::buffer(buffer),
                boost::asio::use_awaitable);

            if (bytes_read > 0) {
                buffer.resize(bytes_read);

                if (messageHandler) {
                    messageHandler(buffer, socket);
                }
            }
        }
    }
    catch (const boost::system::system_error& ex) {
        if (ex.code() != boost::asio::error::eof &&
            ex.code() != boost::asio::error::connection_reset) {
            cerr << "Ошибка чтения от клиента: " << boost::locale::conv::to_utf<char>(ex.what(), "Windows-1251") << endl;
        }
    }
    catch (const exception& ex) {
        cerr << "Ошибка в обработчике клиента: " << ex.what() << endl;
    }

    // Удаляем соединение
    removeConnection(socket);

    if (closeHandler) {
        closeHandler(socket);
    }

    co_return;
}

void ConnectionServer::stop() {
    boost::asio::post(m_ioContext, [this]() {
        boost::system::error_code ec;
        m_acceptor.close(ec);

        lock_guard<mutex> lock(m_connectionsMutex);
        for (auto& socket : m_connections) {
            socket->close(ec);
        }
        m_connections.clear();
    });
}

void ConnectionServer::send(shared_ptr<vector<uint8_t>> pData, shared_ptr<tcp::socket> socket) {
    if (!socket || pData->empty()) {
        return;
    }

    // 1. Копируем данные в кучу СРАЗУ. 
    // Теперь данные "живут" внутри shared_ptr и не зависят от времени жизни исходного span.

    // 2. Захватываем data_ptr по значению. Счётчик ссылок +1.
    boost::asio::post(m_ioContext,
        [this, pData, socket]() {
            if (!socket->is_open()) {
                return;
            }

            // Теперь это безопасно, так как data_ptr владеет памятью
            uint16_t packetType = *(reinterpret_cast<const uint16_t*>(pData->data()));
            cout << "Type: " << packetType << endl;

            boost::asio::async_write(*socket,
                boost::asio::buffer(*pData),
                // 3. Захватываем data_ptr еще раз, чтобы он не удалился, пока идет запись в сокет
                [this, socket, pData](boost::system::error_code ec, size_t bytes) {
                    if (ec) {
                        if (ec != boost::asio::error::operation_aborted) {
                            cerr << "Ошибка отправки: " << ec.message() << endl;
                        }
                        removeConnection(socket);
                    }
                    else {
                        cout << "Отправлено " << bytes << " байт" << endl;
                    }
                    // После завершения этого колбэка data_ptr выйдет из области видимости, 
                    // счетчик упадет до 0, и память очистится. Никаких утечек!
                });
        });
}

void ConnectionServer::broadcastData(shared_ptr<vector<uint8_t>> pData) {
    lock_guard<mutex> lock(m_connectionsMutex);
    for (auto& socket : m_connections) {
        if (socket->is_open()) {
            send(pData, socket);
        }
    }
}

void ConnectionServer::broadcastData(shared_ptr<vector<uint8_t>> pData,
    std::function<bool(const std::shared_ptr <tcp::socket> &) > filter) {
    lock_guard<mutex> lock(m_connectionsMutex);
    for (auto& socket : m_connections) {
        if (socket->is_open() && filter(socket)) {
            send(pData, socket);
        }
    }
}

void ConnectionServer::sendSPackets(span<const SPacket> packets, shared_ptr<tcp::socket> socket) {
    if (!socket || !socket->is_open()) return;
    if (packets.empty()) return;

    const int MAX_IN_FLIGHT = static_cast<int>(packets.size() * 2);
    if (m_packetsInFlight > MAX_IN_FLIGHT) {
        printf("drop\n");
        return;
    }

    m_packetsInFlight += static_cast<int>(packets.size());

    for (size_t i = 0; i < packets.size(); ++i) {
        const SPacket* pkt = &(packets[i]);
        span<const uint8_t> pkt_span(reinterpret_cast<const uint8_t*>(pkt), pkt->dataSize);
        boost::asio::post(m_ioContext,
            [this, pkt_span, socket]() {  // Убрали создание нового span
                if (!socket->is_open()) {
                    return;
                }

                // Копируем данные для асинхронной операции
                auto data_ptr = make_shared<vector<uint8_t>>(pkt_span.begin(), pkt_span.end());

                boost::asio::async_write(*socket,
                    boost::asio::buffer(*data_ptr),
                    [this, socket, data_ptr](boost::system::error_code ec, size_t bytes) {
                        m_packetsInFlight--;
                        if (ec) {
                            if (ec != boost::asio::error::operation_aborted) {
                                cerr << "Ошибка отправки: " << ec.message() << endl;
                            }
                            removeConnection(socket);
                        }
                        else {
                            cout << "Отправлено " << bytes << " байт" << endl;
                            ++m_packetsSent;
                            m_bytesSent += bytes;
                        }
                    });
            });

    }
    ++m_framesSent;
}

set<shared_ptr<tcp::socket>> ConnectionServer::getConnections() {
    return m_connections;
}

void ConnectionServer::setOpenHandler(function<void(shared_ptr<tcp::socket> socket)> openHandler) {
    this->openHandler = openHandler;
}

void ConnectionServer::setMessageHandler(function<void(const vector<uint8_t>& data, shared_ptr<tcp::socket> socket)> messageHandler) {
    this->messageHandler = messageHandler;
}

void ConnectionServer::setClosehandler(function<void(shared_ptr<tcp::socket> socket)> closeHandler) {
    this->closeHandler = closeHandler;
}

void ConnectionServer::addConnection(shared_ptr<tcp::socket> socket) {
    lock_guard<mutex> lock(m_connectionsMutex);
    m_connections.insert(socket);
}

void ConnectionServer::removeConnection(shared_ptr<tcp::socket> socket) {
    lock_guard<mutex> lock(m_connectionsMutex);
    m_connections.erase(socket);
}

