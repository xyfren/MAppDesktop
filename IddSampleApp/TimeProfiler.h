#pragma once

#include <iostream>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>

class TimeProfiler {
public:
    static TimeProfiler& instance() {
        static TimeProfiler inst;
        return inst;
    }

    // «апоминает метку времени дл€ конкретного событи€
    void stamp(const std::string& eventName) {
        //auto now = std::chrono::high_resolution_clock::now();
        //std::lock_guard<std::mutex> lock(mtx);
        //logQueue.push(eventName + ": " + std::to_string(
        //    std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()) + " ms");
        //cv.notify_one();
    }

private:
    TimeProfiler() {
        m_running = true;
        worker = std::thread(&TimeProfiler::processLogs, this);
        worker.detach();
    }

    ~TimeProfiler() {
        m_running = false;
        cv.notify_one();
    }

    void processLogs() {
        while (m_running) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return !logQueue.empty(); });

            while (!logQueue.empty()) {
                std::cout << "[Profiler] " << logQueue.front() << std::endl;
                logQueue.pop();
            }
        }
    }

    std::thread worker;
    std::queue<std::string> logQueue;
    std::mutex mtx;
    std::condition_variable cv;
    bool m_running = false;
};