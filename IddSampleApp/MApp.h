#pragma once

#include "MonitorManager.h"
#include "MServer.h"


class MApp
{
public:
	MApp();
	~MApp();



private:
	MonitorManager* m_pMonitorManager;
	MServer* m_pMServer;
	

	map<MClient, Monitor> m_ClientMonitor;

};

