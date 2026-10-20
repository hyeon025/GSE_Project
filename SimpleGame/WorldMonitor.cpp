#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <iostream>
#include "WorldMonitor.h"
#include "MonitorSnapshot.h"

namespace
{
    unsigned long long Timestamp()
    {
        return (unsigned long long
        )std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    }
}

WorldMonitor::WorldMonitor()
{
    wchar_t base[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, base)))
    {
        std::cerr << "[Monitor] LocalAppData unavailable; export disabled.\n";
        return;
    }
    std::wstring root = std::wstring(base) + L"\\GSE_Project";
    std::wstring directory = root + L"\\Monitor";
    if ((!CreateDirectoryW(root.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) ||
        (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS))
    {
        std::cerr << "[Monitor] Cannot create snapshot directory; export disabled.\n";
        return;
    }
    m_Path = directory + L"\\snapshot.json";
    m_Session = Timestamp() * 65536ULL + GetCurrentProcessId();
    try
    {
        m_Worker = std::thread(&WorldMonitor::WriteLoop, this);
        std::cout << "[Monitor] JSON snapshot export enabled (1 Hz, latest-only).\n";
    }
    catch (const std::exception& error)
    {
        m_Path.clear();
        std::cerr << "[Monitor] Worker unavailable: " << error.what() << '\n';
    }
}

WorldMonitor::~WorldMonitor()
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Stop = true;
    }
    m_Ready.notify_one();
    if (m_Worker.joinable())
    {
        m_Worker.join();
    }
}

void WorldMonitor::Publish(const Game::World& world, bool paused, bool stopped)
{
    auto now = std::chrono::steady_clock::now();
    if (m_Path.empty() || (!stopped && now < m_Next))
    {
        return;
    }
    m_Next = now + std::chrono::seconds(1);
    // Capture on the game thread. The worker never reads mutable gameplay data.
    auto snapshot = Monitoring::Snapshot(world, m_Session, ++m_Sequence, Timestamp(), paused, stopped);
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Pending = std::move(snapshot);
    }
    m_Ready.notify_one();
}

void WorldMonitor::WriteLoop()
{
    bool failed = false;
    for (;;)
    {
        std::string snapshot;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Ready.wait(
                lock,
                [this]
                {
                    return m_Stop || !m_Pending.empty();
                }
            );
            if (m_Pending.empty() && m_Stop)
            {
                return;
            }
            snapshot.swap(m_Pending);
        }
        auto temporary = m_Path + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
        std::ofstream out(temporary.c_str(), std::ios::binary | std::ios::trunc);
        out.write(snapshot.data(), (std::streamsize)snapshot.size());
        out.flush();
        bool ok = out.good();
        out.close();
        ok = ok && !out.fail();
        // A reader may briefly hold the old file on Windows. Retry only on this worker.
        for (int retry = 0; ok && retry < 4; ++retry)
        {
            if (MoveFileExW(temporary.c_str(), m_Path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                break;
            }
            if (retry == 3)
            {
                ok = false;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        }
        if (!ok)
        {
            DeleteFileW(temporary.c_str());
        }
        if (!ok && !failed)
        {
            std::cerr << "[Monitor] Snapshot write failed; retaining previous snapshot.\n";
        }
        else if (ok && failed)
        {
            std::cout << "[Monitor] Snapshot export recovered.\n";
        }
        failed = !ok;
    }
}
