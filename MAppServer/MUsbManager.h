#pragma once

#include <string>
#include <set>
#include <thread>
#include <atomic>
#include <mutex>

class MUsbManager {
public:
    MUsbManager(uint16_t port);
    ~MUsbManager();

    void start();
    void stop();

private:
    uint16_t m_port;
    std::atomic<bool> m_active;
    std::thread m_workerThread;

    // Храним серийники, для которых туннель уже успешно создан
    std::set<std::string> m_activeSerials;
    std::mutex m_mutex;

    void monitorLoop();
    std::set<std::string> fetchAdbDevices();
    std::string execCommand(const std::string& cmd);
};