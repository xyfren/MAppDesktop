#pragma once

#include <iostream>
#include <map>

#include "ConnectionServer.h"
#include "DataServer.h"
#include "MClient.h"
#include "APacket.h"

class MDisplayApp;

class MServer 
{
	friend MDisplayApp;

public:
	MServer();
	~MServer();

	void setup(uint16_t connectionPort, uint16_t dataPort);
	void run();

	void broadcastMessage(const string& msg);

	void onOpen(shared_ptr<tcp::socket> socket);
	void onMessageC(const vector<uint8_t>& data, shared_ptr<tcp::socket> socket);
	void onMessageD(const vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& fromEndpoint);
	void onClose(shared_ptr<tcp::socket> socket);

private:
	boost::asio::io_context m_ioContext;

	int m_connectionPort = 12345;
	int m_dataPort = 12346;

	ConnectionServer* m_connectionServer;
	DataServer* m_dataServer;

	mutex m_clientsMutex;
	map<shared_ptr<tcp::socket>,MClient> m_clients;
};
