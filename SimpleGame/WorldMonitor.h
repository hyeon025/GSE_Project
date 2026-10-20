#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace Game
{
    struct World;
}

class WorldMonitor
{
public:
    WorldMonitor();
    ~WorldMonitor();
    WorldMonitor(const WorldMonitor&) = delete;
    WorldMonitor& operator=(const WorldMonitor&) = delete;
    void Publish(const Game::World& world, bool paused, bool stopped = false);

private:
    void WriteLoop();
    std::wstring m_Path;
    std::mutex m_Mutex;
    std::condition_variable m_Ready;
    std::thread m_Worker;
    std::string m_Pending;
    bool m_Stop = false;
    unsigned long long m_Session = 0, m_Sequence = 0;
    std::chrono::steady_clock::time_point m_Next;
};
