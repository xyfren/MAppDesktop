#pragma once

#include <iostream>
#include <map>

#include "ConnectionServer.h"
#include "DataServer.h"
#include "MClient.h"
#include "APacket.h"
#include "../Common.h"

#include <netioapi.h>

class MDisplayApp;

class MServer 
{
	friend MDisplayApp;

public:
	MServer();
	~MServer();

	string getLocalIpAddress();
	void WINAPI OnAddrChange(PMIB_IPINTERFACE_ROW Row, MIB_NOTIFICATION_TYPE NotificationType);

	void setup(uint16_t connectionPort, uint16_t dataPort);
	void run();

	void broadcastMessage(const string& msg);
	void sendFrame(span<uint8_t> frameData,std::mutex* frameMutex, const udp::endpoint& targetEndpoint);

	void onOpen(shared_ptr<tcp::socket> socket);
	void onMessageC(const vector<uint8_t>& data, shared_ptr<tcp::socket> socket); // connectionServer message
	void onClose(shared_ptr<tcp::socket> socket);

	void onMessageD(const vector<uint8_t>& data, const boost::asio::ip::udp::endpoint& fromEndpoint); // dataServer message
	
	void setCreateMonitorCallback(function<void(MonitorConfig config, std::shared_ptr<MClient> client)> createMonitorCallback);
	void setRemoveMonitorCallback(function<void(std::shared_ptr<MClient> client)> removeMonitorCallback);

	void sendRDPacket();

private:
	function<void(MonitorConfig config, std::shared_ptr<MClient> client)> m_createMonitorCallback;
	function<void(std::shared_ptr<MClient> client)> m_removeMonitorCallback;

	boost::asio::io_context m_ioContext;
	
	string m_serverLocalAddress;
	HANDLE m_hServerLocalAddressChange;
	
	uint16_t m_connectionPort = 12345;
	uint16_t m_dataPort = 12346;

	ConnectionServer* m_connectionServer;
	DataServer* m_dataServer;

	mutex m_clientsMutex;
	map<shared_ptr<tcp::socket>,shared_ptr<MClient>> m_clients;
};
