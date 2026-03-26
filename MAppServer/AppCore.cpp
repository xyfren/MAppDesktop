#include "AppCore.h"
#include "AppCore.h"
#include <chrono>

AppCore::~AppCore() {
    stop();
}

bool AppCore::start() {
    if (m_pMApp) return false;

    m_pMApp = new MApp();

    m_thread = std::thread(&AppCore::worker, this);
    
    return true;
}

void AppCore::stop() {
    m_pMApp->stop();

    if (m_thread.joinable()) m_thread.join();
}

void AppCore::worker() {
    if (m_pMApp->run() != 0) {
        printf("App complete with errors!!!\n");
        printf("Press enter to exit...\n");
        cin.get();

        exit(1);
    }
    else {
        printf("App complete successfully!\n");
        exit(0);
    }
}