#include "MApp.h"

MApp::MApp() {

}

MApp::~MApp() {
	if (m_pMonitorManager) {
		delete m_pMonitorManager;
	}
	if (m_pMServer) {
		delete m_pMonitorManager;
	}
}