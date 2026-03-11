#pragma once

#include <boost/bimap.hpp>
#include <boost/locale.hpp>
#include <memory>

#include "MonitorManager.h"
#include "MServer.h"
#include "FrameManager.h"

class MApp
{
public:
	MApp();
	~MApp();

	int run();

	int eventLoop();

	void createMonitorCallback(MonitorConfig config, std::shared_ptr<MClient> client);
	void removeMonitorCallback(std::shared_ptr<MClient> client);

	void sendFrameCallback(std::shared_ptr<Monitor> pMonitor, uint32_t frameId,uint32_t frameSize, uint32_t rowPitch, void* frameData);

	void testSend(std::shared_ptr<MClient> client);

	
	void PrintIpTable();


private:
	MonitorManager* m_pMonitorManager;
	MServer* m_pMServer;

	std::mutex monitorMutex;
	boost::bimap<std::shared_ptr<Monitor>, std::shared_ptr<MClient>> m_MonitorClient;
	std::map<std::shared_ptr<Monitor>, std::shared_ptr<FrameManager>> m_MonitorFrameManager;

};

