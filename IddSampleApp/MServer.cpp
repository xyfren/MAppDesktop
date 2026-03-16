#include "MServer.h"

#pragma comment(lib, "iphlpapi.lib")

MServer::MServer() {
	m_connectionServer = new ConnectionServer(m_ioContext);
	m_dataServer = new DataServer(m_ioContext);
}

MServer::~MServer() {
    CancelMibChangeNotify2(m_hServerLocalAddressChange);

	delete m_connectionServer;
	delete m_dataServer;
}

std::string MServer::getLocalIpAddress() {
    boost::asio::ip::tcp::resolver resolver(m_ioContext);

    std::string host_name = boost::asio::ip::host_name();

    boost::asio::ip::tcp::resolver::results_type results = resolver.resolve(host_name, "");

    for (auto const& endpoint : results) {
        boost::asio::ip::address addr = endpoint.endpoint().address();
        if (addr.is_v4() && !addr.is_loopback()) {
            return addr.to_string();
        }
    }
    return string();
}

void WINAPI MServer::OnAddrChange(PMIB_IPINTERFACE_ROW Row, MIB_NOTIFICATION_TYPE NotificationType) {
    if (NotificationType == MibParameterNotification) {
        string newAddress = getLocalIpAddress();
        if (newAddress != "" && newAddress != m_serverLocalAddress) {
            cout << "Смена локального IP адреса: " << m_serverLocalAddress << " -> " << newAddress << endl;
			m_serverLocalAddress = newAddress;
        }
    }
        
}

void MServer::setup(uint16_t connectionPort, uint16_t dataPort) {
    DWORD ret = NotifyIpInterfaceChange(AF_UNSPEC, [](PVOID context, PMIB_IPINTERFACE_ROW row, MIB_NOTIFICATION_TYPE type) {
        auto* pApp = static_cast<MServer*>(context);
        pApp->OnAddrChange(row, type);
        }, this, TRUE, &m_hServerLocalAddressChange);

    if (ret != NO_ERROR) {
        printf("Ошибка регистрации: %d\n", ret);
        return ;
    }
    
	m_connectionPort = connectionPort;
	m_dataPort = dataPort;
    m_serverLocalAddress = getLocalIpAddress();

    m_connectionServer->setOpenHandler(bind(&MServer::onOpen, this, placeholders::_1));
    m_connectionServer->setMessageHandler(bind(&MServer::onMessageC, this, placeholders::_1, placeholders::_2));
    m_connectionServer->setClosehandler(bind(&MServer::onClose, this, placeholders::_1));

    m_dataServer->setMessageHandler(bind(&MServer::onMessageD, this, placeholders::_1, placeholders::_2));
}

void MServer::run() {
    cout << "Адрес сервера: " << m_serverLocalAddress << endl;
	m_connectionServer->run(m_connectionPort);
    m_dataServer->run(m_dataPort);
    const int num_threads = 2;
	cout << "Запуск с " << num_threads << " потоками" << endl;
	vector<thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        m_ioContext.run();
    }
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
                if (client->state == MClient::State::Authorized) {
                    m_dataServer->send(udpbytes, client->targetEndpoint);
                }
            }
        }
        catch (const boost::system::system_error& ex) {
            cerr << "Ошибка получения endpoint: " << ex.what() << endl;
        }
    }
}
void MServer::sendSPackets(std::span<const SPacket> packets, const udp::endpoint& targetEndpoint) {
    m_dataServer->sendSPackets(packets, targetEndpoint);
}

void MServer::onOpen(shared_ptr<tcp::socket> socket) {
    {
        lock_guard<mutex> lock(m_clientsMutex);
        m_clients[socket] = make_shared<MClient>(socket);
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
            // Add udp endpoint to MClient 
            boost::asio::ip::tcp::endpoint tcp_ep = socket->remote_endpoint();
            boost::asio::ip::udp::endpoint targetEndpoint(tcp_ep.address(), pack.udpPort);
            m_clients[socket]->targetEndpoint = targetEndpoint;

            m_clients[socket]->state = MClient::State::Authorized;
        }
        RAPacket responsePacket;
        m_connectionServer->send(responsePacket.bytes(), socket);

        MonitorConfig config;
        config.width = pack.width;
        config.height = pack.height;
        config.refreshRate = 60;
        config.coderType = pack.coderType;
        config.connectionType = pack.connectionType;

        m_createMonitorCallback(config,m_clients.at(socket));
    }
}

void MServer::onClose(shared_ptr<tcp::socket> socket) {
    m_removeMonitorCallback(m_clients.at(socket));
    {
        lock_guard<mutex> lock(m_clientsMutex);
        m_clients.erase(socket);
    }
    cout << "Клиент отключился (" << m_clients.size() << " активных)" << endl;
}

void MServer::onMessageD(const vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& fromEndpoint) {
    string str(data.begin(), data.end());
    const uint16_t packetType = *(reinterpret_cast<const uint16_t*>(data.data()));
    if (packetType == 200) {
        cout << "Новое сообщение: " << str << endl;
        cout << "Размер: " << data.size() << endl;

        cout << "Тип пакета: " << packetType << endl;
    }
}

void MServer::setCreateMonitorCallback(function<void(MonitorConfig config, shared_ptr<MClient> client)> createMonitorCallback) {
    m_createMonitorCallback = createMonitorCallback;
}

void MServer::setRemoveMonitorCallback(function<void(shared_ptr<MClient> client)> removeMonitorCallback) {
    m_removeMonitorCallback = removeMonitorCallback;
}

void MServer::sendRDPacket() {
    if (m_serverLocalAddress == "") return;
	if (!m_clients.empty()) return;

    RDPacket packet;
    packet.ipAddress = boost::asio::ip::make_address_v4(m_serverLocalAddress).to_uint();
    packet.connectionPort = m_connectionPort;
    packet.dataPort = m_dataPort;
    boost::asio::ip::udp::endpoint broadcastEndpoint(
        boost::asio::ip::address_v4::broadcast(),
        m_dataPort  // или нужный вам порт
    );
    m_dataServer->send(packet.bytes(), broadcastEndpoint);
}