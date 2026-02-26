#pragma once

#include <iostream>
#include <map>

#include "ConnectionServer.h"
#include "DataServer.h"
#include "MClient.h"
#include "APacket.h"
#include "../Common.h"

class MDisplayApp;

class MServer 
{
	friend MDisplayApp;

public:
	MServer();
	~MServer();

	string getLocalIpAddress(boost::asio::io_context& io_context);

	void setup(uint16_t connectionPort, uint16_t dataPort);
	void run();

	void broadcastMessage(const string& msg);

	void onOpen(shared_ptr<tcp::socket> socket);
	void onMessageC(const vector<uint8_t>& data, shared_ptr<tcp::socket> socket); // connectionServer message
	void onClose(shared_ptr<tcp::socket> socket);

	void onMessageD(const vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& fromEndpoint); // dataServer message
	
	void setCreateMonitorCallback(function<void(MonitorConfig& config, shared_ptr<tcp::socket> socket)> createMonitorCallback);
	void setRemoveMonitorCallback(function<void(MonitorConfig& config, shared_ptr<tcp::socket> socket)> removeMonitorCallback);

private:
	function<void(MonitorConfig& config, shared_ptr<tcp::socket> socket)> m_createMonitorCallback;
	function<void(MonitorConfig& config, shared_ptr<tcp::socket> socket)> m_removeMonitorCallback;

	boost::asio::io_context m_ioContext;
	
	string m_serverLocalAddress;
	
	int m_connectionPort = 12345;
	int m_dataPort = 12346;

	ConnectionServer* m_connectionServer;
	DataServer* m_dataServer;

	mutex m_clientsMutex;
	map<shared_ptr<tcp::socket>,MClient> m_clients;
};
