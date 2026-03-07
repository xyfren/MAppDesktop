#include "MApp.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
	if (!m_pMonitorManager->Initialize()) {
		cout << "Failed to initialize MonitorManager" << endl;	
		return -1;
	}

	m_pMServer->setCreateMonitorCallback(bind(&MApp::createMonitorCallback, this, placeholders::_1, placeholders::_2));
	m_pMServer->setRemoveMonitorCallback(bind(&MApp::removeMonitorCallback, this, placeholders::_1));
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

void MApp::createMonitorCallback(MonitorConfig config, std::shared_ptr<MClient> client) {
	{
		lock_guard<mutex> lock(monitorMutex);
		cout << "New monitor added " << config.width << " " << config.height << endl;
		config.monitorId = m_MonitorClient.size() + 1;
		config.byteDepth = 4;
		config.enabled = true;

		std::shared_ptr<Monitor> newMonitor = make_shared<Monitor>(config);
		newMonitor->setFrameCallback(bind(&MApp::sendFrameCallback, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4));
		
		m_MonitorClient.insert({newMonitor,client});
		m_MonitorFrameManager[newMonitor] = std::make_shared<FrameManager>(config);

		m_pMonitorManager->AddMonitor(newMonitor);
	}
	//sendFrameCallback(m_MonitorClient.right.at(client), 0, );
	//testSend(client);
}

void MApp::testSend(std::shared_ptr<MClient> client) {
	int width, height, channels;

	// 1. Получаем информацию о файле перед полной загрузкой
	if (!stbi_info("input.png", &width, &height, &channels)) {
		std::cerr << "Could not read file info" << std::endl;
		return ;
	}

	unsigned char* imgData = stbi_load("input.png", &width, &height, &channels, 4);
	if (!imgData) {
		std::cerr << "Could not load PNG file" << std::endl;
		return ;
	}
	sendFrameCallback(m_MonitorClient.right.at(client), 0,width*height*channels,imgData);
	stbi_image_free(imgData);
}

void MApp::removeMonitorCallback(std::shared_ptr<MClient> client) {
	{
		lock_guard<mutex> lock(monitorMutex);
		cout << "Monitor removed" << endl;
		auto rightIt = m_MonitorClient.right.find(client);
		if (rightIt != m_MonitorClient.right.end()) {
			m_pMonitorManager->RemoveMonitor(rightIt->second->GetConfig().monitorId);

			m_MonitorFrameManager.erase(rightIt->second);
			m_MonitorClient.right.erase(rightIt);
		}
		
	}
}

void MApp::sendFrameCallback(std::shared_ptr<Monitor> pMonitor, uint32_t frameId, uint32_t frameSize, void* frameData) {
	cout << "send frame" << endl;
	std::span<uint8_t> inputBuffer((uint8_t*)frameData, frameSize);
	std::span<uint8_t> outputBuffer;
	std::mutex* outputMutex = nullptr;
	{
		lock_guard<mutex> lock(monitorMutex);
		std::shared_ptr<FrameManager> frameManager = m_MonitorFrameManager.at(pMonitor);
		int r = frameManager->createFrameBuffer(frameId, inputBuffer, &outputMutex, outputBuffer);	
		
		if (r != 0) {
			return;
		}
	}
	m_pMServer->sendFrame(outputBuffer,outputMutex,m_MonitorClient.left.at(pMonitor)->targetEndpoint);

}