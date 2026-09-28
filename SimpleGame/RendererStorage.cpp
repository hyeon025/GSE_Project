#include "stdafx.h"
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "Renderer.h"

namespace
{
    bool Folder(const std::wstring& path)
    {
        return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
    }

    size_t CacheFileCount(const std::wstring& directory)
    {
        WIN32_FIND_DATAW entry = {};
        HANDLE search = FindFirstFileW((directory + L"\\*.mesh").c_str(), &entry);
        if (search == INVALID_HANDLE_VALUE)
        {
            return 0;
        }
        size_t count = 0;
        do
        {
            if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                ++count;
            }
        }
        while (FindNextFileW(search, &entry));
        FindClose(search);
        return count;
    }

    void TrimOldLogs(const std::wstring& directory)
    {
        struct File
        {
            std::wstring path;
            std::uint64_t size, time;
        };

        std::vector<File> files;
        std::uint64_t total = 0;
        WIN32_FIND_DATAW entry = {};
        HANDLE search = FindFirstFileW((directory + L"\\render_*.csv").c_str(), &entry);
        if (search == INVALID_HANDLE_VALUE)
        {
            return;
        }
        do
        {
            if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }
            std::uint64_t size = (std::uint64_t(entry.nFileSizeHigh) << 32) | entry.nFileSizeLow;
            std::uint64_t time =
                (std::uint64_t(entry.ftLastWriteTime.dwHighDateTime) << 32) | entry.ftLastWriteTime.dwLowDateTime;
            files.push_back({directory + L"\\" + entry.cFileName, size, time});
            total += size;
        }
        while (FindNextFileW(search, &entry));
        FindClose(search);
        std::sort(
            files.begin(),
            files.end(),
            [](const File& a, const File& b)
            {
                return a.time < b.time;
            }
        );
        size_t remaining = files.size();
        for (const auto& file : files)
        {
            if (total <= 128ull * 1024 * 1024 && remaining <= 64)
            {
                break;
            }
            if (DeleteFileW(file.path.c_str()))
            {
                total -= file.size;
                --remaining;
            }
        }
    }
}

void Renderer::InitializeStorage()
{
    wchar_t local[32768] = {};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768);
    if (!length || length >= 32768)
    {
        std::cerr << "[Render] LOCALAPPDATA unavailable: disk cache and file profiling disabled.\n";
        return;
    }
    std::wstring root = std::wstring(local) + L"\\GSE_Project";
    if (!Folder(root))
    {
        std::cerr << "[Render] Cannot create storage directory.\n";
        return;
    }
    std::wstring cache = root + L"\\RendererCache";
    std::wstring revision = cache + L"\\v" + std::to_wstring(RenderMeshCache::Revision);
    if (Folder(cache) && Folder(revision))
    {
        m_CacheDirectory = revision;
        m_DiskCacheFiles = CacheFileCount(revision);
    }
    else
    {
        std::cerr << "[Render] Mesh disk cache unavailable; using memory cache.\n";
    }
    std::wstring logs = root + L"\\RenderLogs";
    if (!Folder(logs))
    {
        std::cerr << "[Render] File profiling unavailable.\n";
        return;
    }
    TrimOldLogs(logs);
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    std::wostringstream name;
    name << logs << L"\\render_" << time.wYear << L'_' << time.wMonth << L'_' << time.wDay << L'_' << time.wHour << L'_'
         << time.wMinute << L'_' << time.wSecond << L'_' << time.wMilliseconds << L'_' << GetCurrentProcessId();
    m_LogBase = name.str();
    OpenProfileSegment();
    m_EventLog.open((m_LogBase + L"_events_0.csv").c_str(), std::ios::binary | std::ios::trunc);
    if (m_EventLog)
    {
        m_EventLog << "frame,event,mesh_kind,variant,vertices,elapsed_us\n";
    }
    else
    {
        std::cerr << "[Render] Could not open cache event log.\n";
    }
    std::wcout << L"[Render] Profile logs: " << m_LogBase << L"_*.csv\n";
}

bool Renderer::LoadMeshFile(MeshKey key, std::vector<Vertex>& vertices)
{
    if (m_CacheDirectory.empty())
    {
        m_Stats.Add(RenderCounter::DiskMisses);
        return false;
    }
    auto start = std::chrono::steady_clock::now();
    auto path =
        m_CacheDirectory + L"\\" + std::to_wstring((unsigned)key.first) + L"_" + std::to_wstring(key.second) + L".mesh";
    std::ifstream in(path.c_str(), std::ios::binary);
    bool opened = static_cast<bool>(in);
    bool loaded = opened && RenderMeshCache::Load(in, (unsigned)key.first, key.second, vertices);
    auto ioUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    m_Stats.Add(RenderCounter::CacheIoUs, ioUs);
    m_Stats.Add(loaded ? RenderCounter::DiskHits : opened ? RenderCounter::DiskRejected : RenderCounter::DiskMisses);
    CacheEvent(loaded ? "disk_hit" : opened ? "disk_rejected" : "disk_miss", key, loaded ? vertices.size() : 0, ioUs);
    return loaded;
}

void Renderer::SaveMeshFile(MeshKey key, const std::vector<Vertex>& vertices)
{
    if (m_CacheDirectory.empty())
    {
        return;
    }
    auto path =
        m_CacheDirectory + L"\\" + std::to_wstring((unsigned)key.first) + L"_" + std::to_wstring(key.second) + L".mesh";
    bool exists = GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (!exists && m_DiskCacheFiles >= 2048)
    {
        CacheEvent("disk_budget_skip", key, vertices.size());
        return;
    }
    auto start = std::chrono::steady_clock::now();
    auto temporary = path + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    std::ofstream out(temporary.c_str(), std::ios::binary | std::ios::trunc);
    bool ok = out && RenderMeshCache::Save(out, (unsigned)key.first, key.second, vertices);
    out.flush();
    ok = ok && static_cast<bool>(out);
    out.close();
    ok = ok && !out.fail();
    if (ok)
    {
        ok = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }
    if (!ok)
    {
        DeleteFileW(temporary.c_str());
    }
    else if (!exists)
    {
        ++m_DiskCacheFiles;
    }
    m_Stats.Add(ok ? RenderCounter::DiskWrites : RenderCounter::DiskWriteErrors);
    auto ioUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    m_Stats.Add(RenderCounter::CacheIoUs, ioUs);
    CacheEvent(ok ? "disk_write" : "disk_write_error", key, vertices.size(), ioUs);
}

void Renderer::CacheEvent(const char* event, MeshKey key, size_t vertices, std::uint64_t elapsedUs)
{
    if (!m_EventLog.is_open() || !m_EventLog)
    {
        return;
    }
    if (m_EventLog.tellp() >= std::streamoff(4 * 1024 * 1024))
    {
        m_EventLog.close();
        m_EventLog.clear();
        ++m_EventSegment;
        m_EventLog.open(
            (m_LogBase + L"_events_" + std::to_wstring(m_EventSegment % 2) + L".csv").c_str(),
            std::ios::binary | std::ios::trunc
        );
        m_EventLog << "frame,event,mesh_kind,variant,vertices,elapsed_us\n";
    }
    m_EventLog << m_FrameNumber << ',' << event << ',' << (unsigned)key.first << ',' << key.second << ',' << vertices
               << ',' << elapsedUs << '\n';
    if (!m_EventLog)
    {
        std::cerr << "[Render] Cache event log failed; event logging disabled.\n";
    }
}
