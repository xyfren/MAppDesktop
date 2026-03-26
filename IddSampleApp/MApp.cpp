#include "MApp.h"

VOID WINAPI
CreationCallback(
	_In_ HSWDEVICE hSwDevice,
	_In_ HRESULT hrCreateResult,
	_In_opt_ PVOID pContext,
	_In_opt_ PCWSTR pszDeviceInstanceId
);

#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <ws2tcpip.h>


MApp::MApp() {
	m_pMonitorManager = new MonitorManager();
	m_pMServer = new MServer();
	TimeProfiler::instance();
}

MApp::~MApp() {
	if (m_pMonitorManager) {
		delete m_pMonitorManager;
	}

	if (m_pMServer) {
		delete m_pMServer;
	}

	if (m_pMUsbManager) {
		delete m_pMUsbManager;
	}
}

int MApp::initDevice() {
	HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!hEvent) {
		printf("Failed to create event!\n");
		return 1;
	}
	
	SW_DEVICE_CREATE_INFO createInfo = { 0 };

	createInfo.cbSize = sizeof(createInfo);
	createInfo.pszzCompatibleIds = L"MAppDriver\0\0";
	createInfo.pszInstanceId = L"MAppDriver";
	createInfo.pszzHardwareIds = L"MAppDriver\0\0";
	createInfo.pszDeviceDescription = L"MApp Driver";
	createInfo.CapabilityFlags = SWDeviceCapabilitiesRemovable |
		SWDeviceCapabilitiesSilentInstall |
		SWDeviceCapabilitiesDriverRequired;

	// Create the device
	HRESULT hr = SwDeviceCreate(L"MAppDriver",
		L"HTREE\\ROOT\\0",
		&createInfo,
		0,
		nullptr,
		CreationCallback,
		&hEvent,
		&m_hSwDevice);

	if (FAILED(hr))
	{
		printf("SwDeviceCreate failed with 0x%lx\n", hr);
		CloseHandle(hEvent);
		return 1;
	}

	printf("Waiting for device to be created....\n");
	DWORD waitResult = WaitForSingleObject(hEvent, 10000);

	if (waitResult != WAIT_OBJECT_0)
	{
		printf("Wait for device creation failed.\n");
		return 1;
	}
	printf("Device created successfuly!\n");
	
	return 0;
}

int MApp::initServer() {
	m_pMServer->setCreateMonitorCallback(bind(&MApp::createMonitorCallback, this, placeholders::_1, placeholders::_2));
	m_pMServer->setRemoveMonitorCallback(bind(&MApp::removeMonitorCallback, this, placeholders::_1));
	m_pMServer->setup(12345, 12346);

	return 0;
}

int MApp::initUsbManager() {
	m_pMUsbManager = new MUsbManager(12345);

	return 0;
}

int MApp::initMonitorManager() {
	if (!m_pMonitorManager->Initialize()) {
		printf("Failed to initialize MonitorManager!\n");
		return 1;
	}

	return 0;
}

int MApp::run() {
	if (initDevice() != 0) {
		return 1;
	}

	if (initMonitorManager() != 0) {
		return 1;
	}

	if (initServer() != 0) {
		return 1;
	}

	if (initUsbManager() != 0) {
		return 1;
	}

	thread serverThread([this]() {
		m_pMServer->run();
	});
	serverThread.detach();

	m_pMUsbManager->start();

	m_running = true;
	return eventLoop();
}

void MApp::stop() {
	m_running = false;
	m_cv.notify_one();

	while (!m_finished);

	return;
}

int MApp::eventLoop() {
	std::unique_lock<std::mutex> lock(m_mutex);

	while (m_running) {
		m_cv.wait_for(lock, 3000ms, [this] {
			return !m_running; // проснуться, если остановили
		});

		if (!m_running) break;

		lock.unlock();

		try {
			m_pMServer->sendRDPacket();
		}
		catch (const exception& ex) {
			cerr << "Ошибка отправки: " << boost::locale::conv::to_utf<char>(ex.what(), "Windows-1251") << endl;
		}

		lock.lock();
	}
	m_finished = true;
	return 0;
}

void MApp::createMonitorCallback(MonitorConfig config, std::shared_ptr<MClient> client) {
	{
		lock_guard<mutex> lock(monitorMutex);
		cout << "New monitor added " << config.width << " " << config.height << " " << config.refreshRate << endl;
		config.monitorId = m_MonitorClient.size();
		config.byteDepth = 4;
		config.enabled = true;

		std::shared_ptr<Monitor> newMonitor = make_shared<Monitor>(config);
		newMonitor->setFrameCallback(bind(&MApp::sendFrameCallback, this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4, placeholders::_5));
		
		m_MonitorClient.insert({newMonitor,client});
		m_MonitorFrameManager[newMonitor] = make_shared<FrameManager>(config);

		m_pMonitorManager->AddMonitor(newMonitor);
	}
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

void MApp::sendFrameCallback(std::shared_ptr<Monitor> pMonitor, uint64_t frameId, uint32_t frameSize, uint32_t rowPitch, void* frameData) {
	std::shared_ptr<FrameManager> frameManager;
	udp::endpoint targetEndpoint;
	shared_ptr<tcp::socket> socket;
	{
		lock_guard<mutex> lock(monitorMutex);
		auto it = m_MonitorFrameManager.find(pMonitor);
		if (it == m_MonitorFrameManager.end()) return;
		frameManager = it->second;
		targetEndpoint = m_MonitorClient.left.at(pMonitor)->targetEndpoint;
		socket = m_MonitorClient.left.at(pMonitor)->socket;
	}

	auto packets = frameManager->encodeFrame(
		frameId, rowPitch,
		static_cast<const uint8_t*>(frameData), frameSize);

	if (!packets.empty()) {
		if (pMonitor->GetConfig().connectionType == ConnectionType::Wireless) {
			m_pMServer->sendSPackets(packets, targetEndpoint);
		}
		else if (pMonitor->GetConfig().connectionType == ConnectionType::Usb) {
			//cout << "Send packets\n";
			m_pMServer->sendSPackets(packets, socket);
		}
		else {
			cout << "Тип соединения не установлен" << endl;
		}
	}
}

VOID WINAPI
CreationCallback(
	_In_ HSWDEVICE hSwDevice,
	_In_ HRESULT hrCreateResult,
	_In_opt_ PVOID pContext,
	_In_opt_ PCWSTR pszDeviceInstanceId
)
{
	HANDLE hEvent = *(HANDLE*)pContext;
	SetEvent(hEvent);
	UNREFERENCED_PARAMETER(hSwDevice);
	UNREFERENCED_PARAMETER(hrCreateResult);
	UNREFERENCED_PARAMETER(pszDeviceInstanceId);
}