#include "MServer.h"

MServer::MServer() {
	m_connectionServer = new ConnectionServer(m_ioContext);
	m_dataServer = new DataServer(m_ioContext);
}

MServer::~MServer() {
	delete m_connectionServer;
	delete m_dataServer;
}

void MServer::setup(uint16_t connectionPort, uint16_t dataPort) {
	m_connectionPort = connectionPort;
	m_dataPort = dataPort;

    m_connectionServer->setOpenHandler(bind(&MServer::onOpen, this, placeholders::_1));
    m_connectionServer->setMessageHandler(bind(&MServer::onMessageC, this, placeholders::_1, placeholders::_2));
    m_connectionServer->setClosehandler(bind(&MServer::onClose, this, placeholders::_1));

    m_dataServer->setMessageHandler(bind(&MServer::onMessageD, this, placeholders::_1, placeholders::_2));
}

void MServer::run() {
	m_connectionServer->run(m_connectionPort);
    m_dataServer->run(m_dataPort);
	m_ioContext.run();
}

void MServer::broadcastMessage(const string& msg) {
    string tcpstr = "TCP: " + msg;
    vector<uint8_t> tcpbytes(tcpstr.begin(), tcpstr.end());
    string udpstr = "UDP: " + msg;
    vector<uint8_t> udpbytes(udpstr.begin(), udpstr.end());
    
    for (auto& [socket,client] : m_clients) {
        try {
            if (socket and socket->is_open()) {
                m_connectionServer->send(tcpbytes, socket);
                if (client.state == MClient::State::Authorized) {
                    boost::asio::ip::tcp::endpoint tcp_ep = socket->remote_endpoint();
                    boost::asio::ip::udp::endpoint targetEndpoint(tcp_ep.address(),client.udpPort);
                    m_dataServer->send(udpbytes, targetEndpoint);
                }
            }
        }
        catch (const boost::system::system_error& ex) {
            cerr << "Ошибка получения endpoint: " << ex.what() << endl;
        }
    }
}

void MServer::onOpen(shared_ptr<tcp::socket> socket) {
    MClient client(socket);
    {
        lock_guard<mutex> lock(m_clientsMutex);
        m_clients[socket] = client;
    }
    cout << "Новый клиент подключён (" << m_clients.size() << " активных)" << endl;
}

void MServer::onMessageC(const vector<uint8_t>& data, shared_ptr<tcp::socket> socket) {
    string str(data.begin(), data.end());
    cout << "Новое сообщение: " << str << endl;
    cout << "Размер: " << data.size() << endl;
    APacket pack = APacket::fromBytes(data);
    cout << pack.type << " " << pack.udpPort << endl;
    if (pack.type == 100) {
        cout << "Клиаент авторизован" << endl;
        {
            lock_guard<mutex> lock(m_clientsMutex);
            m_clients[socket].udpPort = pack.udpPort;
            m_clients[socket].state = MClient::State::Authorized;
        }
    }

}

void MServer::onClose(shared_ptr<tcp::socket> socket) {
    {
        lock_guard<mutex> lock(m_clientsMutex);
        m_clients.erase(socket);
    }
    cout << "Клиент отключился (" << m_clients.size() << " активных)" << endl;
}

void MServer::onMessageD(const vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& fromEndpoint) {
    string str(data.begin(), data.end());
    cout << "Новое сообщение: " << str << endl;
    cout << "Размер: " << data.size() << endl;
    const uint16_t packetType = *(reinterpret_cast<const uint16_t*>(data.data()));
    cout << "Тип пакета: " << endl;
}