#include "MApp.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

void MApp::PrintIpTable() {
	PMIB_IPADDRTABLE pIPAddrTable = NULL;
	DWORD dwSize = 0;

	// Резервируем память (как в предыдущем примере)
	GetIpAddrTable(NULL, &dwSize, 0);
	pIPAddrTable = (MIB_IPADDRTABLE*)malloc(dwSize);

	if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) == NO_ERROR) {
		for (int i = 0; i < (int)pIPAddrTable->dwNumEntries; i++) {
			char szIPAddr[INET_ADDRSTRLEN]; // Буфер для строки IP

			// Конвертируем DWORD адрес в строку
			// Используем &pIPAddrTable->table[i].dwAddr напрямую
			InetNtopA(AF_INET, &(pIPAddrTable->table[i].dwAddr), szIPAddr, INET_ADDRSTRLEN);

			printf("Запись [%d]: %s\n", i, szIPAddr);
		}
	}
	if (pIPAddrTable) free(pIPAddrTable);
}


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
	HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	HSWDEVICE hSwDevice;
	SW_DEVICE_CREATE_INFO createInfo = { 0 };

	createInfo.cbSize = sizeof(createInfo);
	createInfo.pszzCompatibleIds = L"IddSampleDriver\0\0";
	createInfo.pszInstanceId = L"IddSampleDriver";
	createInfo.pszzHardwareIds = L"IddSampleDriver\0\0";
	createInfo.pszDeviceDescription = L"Idd Sample Driver";
	createInfo.CapabilityFlags = SWDeviceCapabilitiesRemovable |
		SWDeviceCapabilitiesSilentInstall |
		SWDeviceCapabilitiesDriverRequired;

	// Create the device
	HRESULT hr = SwDeviceCreate(L"IddSampleDriver",
		L"HTREE\\ROOT\\0",
		&createInfo,
		0,
		nullptr,
		CreationCallback,
		&hEvent,
		&hSwDevice);
	if (FAILED(hr))
	{
		printf("SwDeviceCreate failed with 0x%lx\n", hr);
		return 1;
	}

	// Wait for callback to signal that the device has been created
	printf("Waiting for device to be created....\n");
	DWORD waitResult = WaitForSingleObject(hEvent, 10000);
	if (waitResult != WAIT_OBJECT_0)
	{
		printf("Wait for device creation failed\n");
		return 1;
	}
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

	m_pMUsbManager = new MUsbManager(12345);
	m_pMUsbManager->start();

	return eventLoop();

}

int MApp::eventLoop() {
	while (true) {
		this_thread::sleep_for(3000ms);
		try {
			//m_mServer->broadcastMessage("Hi Misha My Friend");
			m_pMServer->sendRDPacket();
		}
		catch (const exception& ex) {
			cerr << "Ошибка отправки: " << boost::locale::conv::to_utf<char>(ex.what(), "Windows-1251") << endl;

		}
	}
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
	sendFrameCallback(m_MonitorClient.right.at(client), 0,width*height*channels,0,imgData);
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