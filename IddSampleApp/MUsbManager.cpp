#include "MUsbManager.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <array>
#include <chrono>

MUsbManager::MUsbManager(uint16_t port)
    : m_port(port), m_active(false) {
}

MUsbManager::~MUsbManager() {
    stop();
}

void MUsbManager::start() {
    if (m_active) return;
    m_active = true;
    m_workerThread = std::thread(&MUsbManager::monitorLoop, this);
}

void MUsbManager::stop() {
    if (!m_active) return;
    m_active = false;
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

std::string MUsbManager::execCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// Получение списка серийных номеров устройств, готовых к работе
std::set<std::string> MUsbManager::fetchAdbDevices() {
    std::set<std::string> devices;
    std::string output = execCommand("adb.exe devices");
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line.find("List of devices") != std::string::npos) continue;

        size_t tabPos = line.find('\t');
        if (tabPos != std::string::npos) {
            std::string state = line.substr(tabPos + 1);
            state.erase(std::remove_if(state.begin(), state.end(), ::isspace), state.end());

            if (state == "device") {
                devices.insert(line.substr(0, tabPos));
            }
        }
    }
    return devices;
}

// Основной цикл: просыпается каждые 500мс и синхронизирует состояние
void MUsbManager::monitorLoop() {
    while (m_active) {
        auto currentDevices = fetchAdbDevices();

        std::lock_guard<std::mutex> lock(m_mutex);

        // 1. Убираем устройства, которые были отключены
        for (auto it = m_activeSerials.begin(); it != m_activeSerials.end(); ) {
            if (currentDevices.find(*it) == currentDevices.end()) {
                std::cout << "[MUsbManager] Устройство отключено: " << *it << std::endl;
                it = m_activeSerials.erase(it);
            }
            else {
                ++it;
            }
        }

        // 2. Создаем туннели для новых устройств
        for (const auto& serial : currentDevices) {
            if (m_activeSerials.find(serial) == m_activeSerials.end()) {

                std::string cmd = "adb.exe -s " + serial + " reverse tcp:" +
                    std::to_string(m_port) + " tcp:" + std::to_string(m_port);

                std::string result = execCommand(cmd);

                // Проверяем, не вернул ли adb ошибку (например, порт занят)
                if (result.find("error") == std::string::npos) {
                    m_activeSerials.insert(serial);
                    std::cout << "[MUsbManager] Туннель открыт для устройства: " << serial << std::endl;
                }
                else {
                    // Если ошибка, в следующий такт (через 0.5с) он попробует снова
                    std::cerr << "[MUsbManager] Ошибка adb reverse для " << serial << ": " << result;
                }
            }
        }

        // Спим полсекунды перед следующим опросом
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}