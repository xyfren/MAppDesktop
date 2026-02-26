#include "MApp.h"

MApp::MApp() {
	m_pMonitorManager = new MonitorManager();
	m_pMServer = new MServer();
}

MApp::~MApp() {
	if (m_pMonitorManager) {
		delete m_pMonitorManager;
	}

	if (m_pMServer) {
		delete m_pMServer;
	}
}

int MApp::run() {
	m_pMServer->setCreateMonitorCallback(bind(&MApp::createMonitorCallback, this, placeholders::_1, placeholders::_2));
	m_pMServer->setRemoveMonitorCallback(bind(&MApp::removeMonitorCallback, this, placeholders::_1, placeholders::_2));
	m_pMServer->setup(12345, 12346);
	thread serverThread([this]() {
		m_pMServer->run();
		});
	serverThread.detach();

	return eventLoop();

}

int MApp::eventLoop() {

	while (true) {
		this_thread::sleep_for(1000ms);
		//m_mServer->broadcastMessage("Hi Misha My Friend");
	}
	return 0;
}

void MApp::createMonitorCallback(MonitorConfig& config, shared_ptr<tcp::socket> socket) {
	cout << "New monitor added" << endl;
}

void MApp::removeMonitorCallback(MonitorConfig& config, shared_ptr<tcp::socket> socket) {
	cout << "Monitor removed" << endl;
}

void MApp::sendFrameCallback(Monitor* pMonitor, std::vector<uint8_t> frameData){
	cout << "send frame" << endl;
}