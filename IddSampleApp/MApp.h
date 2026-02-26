#pragma once

#include "MonitorManager.h"
#include "MServer.h"
#include <boost/bimap.hpp>


class MApp
{
public:
	MApp();
	~MApp();

	int run();

	int eventLoop();

	void createMonitorCallback(MonitorConfig& config, shared_ptr<tcp::socket> socket);
	void removeMonitorCallback(MonitorConfig& config, shared_ptr<tcp::socket> socket);

	void sendFrameCallback(Monitor* pMonitor, std::vector<uint8_t> frameData);

private:
	MonitorManager* m_pMonitorManager;
	MServer* m_pMServer;

	boost::bimap<MClient, Monitor*> m_ClientMonitor;

};

