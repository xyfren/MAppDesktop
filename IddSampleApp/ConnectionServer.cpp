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
    catch (const boost::system::system_error& e) {
        if (e.code() != boost::asio::error::eof &&
            e.code() != boost::asio::error::connection_reset) {
            cerr << "Ошибка чтения от клиента: " << e.what() << endl;
        }
    }
    catch (const exception& e) {
        cerr << "Ошибка в обработчике клиента: " << e.what() << endl;
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

void ConnectionServer::send(const vector<uint8_t>& data, shared_ptr<tcp::socket> socket) {
    if (!socket || data.empty()) {
        return;
    }

    // ВСЁ! Просто async_write через strand
    boost::asio::post(m_ioContext,
        [this, data = vector<uint8_t>(data), socket]() mutable {
            if (!socket->is_open()) {
                return;
            }

            // Копируем данные для асинхронной операции
            auto data_ptr = make_shared<vector<uint8_t>>(move(data));

            boost::asio::async_write(*socket,
                boost::asio::buffer(*data_ptr),
                [this, socket, data_ptr](boost::system::error_code ec, size_t bytes) {
                    if (ec) {
                        if (ec != boost::asio::error::operation_aborted) {
                            cerr << "Ошибка отправки: " << ec.message() << endl;
                        }
                        removeConnection(socket);
                    }
                    else {
                         cout << "Отправлено " << bytes << " байт" << endl;
                    }
                });
        });
}

void ConnectionServer::broadcastData(const vector<uint8_t>& data) {
    lock_guard<mutex> lock(m_connectionsMutex);
    for (auto& socket : m_connections) {
        if (socket->is_open()) {
            send(data, socket);
        }
    }
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

