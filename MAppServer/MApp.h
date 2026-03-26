#pragma once

#include <boost/bimap.hpp>
#include <boost/locale.hpp>
#include <memory>
#include <mutex>
#include <condition_variable>

#include "MonitorManager.h"
#include "MServer.h"
#include "MUsbManager.h"
#include "FrameManager.h"
#include "TimeProfiler.h"


class MApp
{
public:
	MApp();
	~MApp();

	int run();
	void stop();

	int eventLoop();

	void createMonitorCallback(MonitorConfig config, std::shared_ptr<MClient> client);
	void removeMonitorCallback(std::shared_ptr<MClient> client);
	void sendFrameCallback(std::shared_ptr<Monitor> pMonitor, uint64_t frameId, uint32_t frameSize, uint32_t rowPitch, void* frameData);

private:
	int initDevice();
	int initUsbManager();
	int initMonitorManager();
	int initServer();

	atomic<bool> m_running = false;
	std::condition_variable m_cv;
	std::mutex m_mutex;
	atomic<bool> m_finished = false;

	HSWDEVICE m_hSwDevice;

	MonitorManager* m_pMonitorManager = nullptr;
	MServer* m_pMServer = nullptr;
	MUsbManager* m_pMUsbManager = nullptr;

	std::mutex monitorMutex;
	boost::bimap<std::shared_ptr<Monitor>, std::shared_ptr<MClient>> m_MonitorClient;
	std::map<std::shared_ptr<Monitor>, std::shared_ptr<FrameManager>> m_MonitorFrameManager;
};

