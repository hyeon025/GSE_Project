#include "stdafx.h"
#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include "Renderer.h"

void Renderer::OpenProfileSegment()
{
    if (m_LogBase.empty() || m_LogFailed)
    {
        return;
    }
    m_FrameLog.close();
    m_FrameLog.clear();
    m_FrameLog.imbue(std::locale::classic());
    m_FrameLog.open(
        (m_LogBase + L"_frames_" + std::to_wstring(m_LogSegment % 4) + L".csv").c_str(),
        std::ios::binary | std::ios::trunc
    );
    if (!m_FrameLog)
    {
        m_LogFailed = true;
        std::cerr << "[Render] Cannot write frame CSV; console profiling remains enabled.\n";
        return;
    }
    const auto* renderer = glGetString(GL_RENDERER);
    const auto* version = glGetString(GL_VERSION);
    m_FrameLog << "# schema=2,mesh_revision=" << RenderMeshCache::Revision
               << ",max_instances=" << RenderBatchLimits::Instances
               << ",padding_ratio=" << RenderBatchLimits::PaddingRatio
               << ",large_mesh_padding_ratio=" << RenderBatchLimits::LargeMeshPaddingRatio
               << ",large_mesh_vertices=" << RenderBatchLimits::LargeMeshVertices << '\n';
    m_FrameLog << "# gpu=" << (renderer ? reinterpret_cast<const char*>(renderer) : "unknown")
               << ",gl=" << (version ? reinterpret_cast<const char*>(version) : "unknown") << '\n';
    m_FrameLog << "# build_mode="
#ifdef _DEBUG
               << "Debug"
#else
               << "Release"
#endif
               << '\n';
    m_FrameLog << "# build=" << __DATE__ << ' ' << __TIME__ << ",shader_fnv64=" << m_ShaderFingerprint
               << ",segment=" << m_LogSegment << '\n';
    m_FrameLog
        << "frame,elapsed_ms,frame_interval_ms,cpu_render_ms,previous_log_ms,width,height,hdr,post_enabled,combat,camera_x,camera_y,zoom,seed"
        << ",mesh_count,mesh_used_bytes,mesh_capacity_bytes,text_entries,text_pages,draw_calls";
    const char* names[] = {"draw_arrays",     "draw_instanced",     "world_draws",
                           "post_draws",      "ui_draws",           "instances",
                           "useful_vertices", "submitted_vertices", "upload_bytes",
                           "mesh_hits",       "mesh_misses",        "mesh_builds",
                           "disk_hits",       "disk_misses",        "disk_rejected",
                           "disk_writes",     "disk_write_errors",  "mesh_evictions",
                           "text_hits",       "text_misses",        "texture_upload_bytes",
                           "flush_explicit",  "flush_pass",         "flush_texture",
                           "flush_capacity",  "flush_padding",      "flush_cache",
                           "empty_flushes",   "mesh_build_us",      "cache_io_us"};
    static_assert(
        sizeof(names) / sizeof(names[0]) == static_cast<size_t>(RenderCounter::Count),
        "Counter schema mismatch."
    );
    for (auto name : names)
    {
        m_FrameLog << ',' << name;
    }
    m_FrameLog << ",game_update_ms,previous_swap_ms\n";
}

void Renderer::WriteProfileFrame(double renderMs)
{
    if (!m_FrameLog.is_open() || m_LogFailed)
    {
        return;
    }
    if (m_FrameLog.tellp() >= std::streamoff(8 * 1024 * 1024))
    {
        ++m_LogSegment;
        OpenProfileSegment();
    }
    m_FrameLog << std::fixed << std::setprecision(3) << m_FrameNumber << ',' << m_FrameElapsedMs << ','
               << m_FrameIntervalMs << ',' << renderMs << ',' << m_PreviousLogMs << ',' << m_Width << ',' << m_Height
               << ',' << m_TargetsReady << ',' << m_PostSettings.enabled << ',' << m_ProfileCombat << ','
               << m_ProfileCameraX << ',' << m_ProfileCameraY << ',' << m_ProfileZoom << ',' << m_ProfileSeed << ','
               << m_Meshes.size() << ',' << m_MeshUsed * sizeof(Vertex) << ',' << m_MeshCapacity * sizeof(Vertex) << ','
               << m_Text.size() << ',' << m_TextPages.size() << ',' << m_FrameDrawCalls;
    for (auto value : m_Stats.counters)
    {
        m_FrameLog << ',' << value;
    }
    m_FrameLog << ',' << m_GameUpdateMs << ',' << m_PreviousSwapMs << '\n';
    if (!m_FrameLog)
    {
        m_LogFailed = true;
        std::cerr << "[Render] Frame CSV write failed; console profiling remains enabled.\n";
    }
}

void Renderer::BeginFrame()
{
    m_DepthLayer = 0;
    // HUD and avatar previews render to the default target after HDR composition.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDepthMask(GL_TRUE);
    glClearDepth(1);
    glClear(GL_DEPTH_BUFFER_BIT);
    m_FrameStart = std::chrono::steady_clock::now();
    if (m_FrameNumber == 0)
    {
        m_ProfileStart = m_SessionStart = m_PreviousFrameStart = m_FrameStart;
    }
    m_FrameElapsedMs = std::chrono::duration<double, std::milli>(m_FrameStart - m_SessionStart).count();
    m_FrameIntervalMs = std::chrono::duration<double, std::milli>(m_FrameStart - m_PreviousFrameStart).count();
    m_PreviousFrameStart = m_FrameStart;
    ++m_FrameNumber;
    m_FrameDrawCalls = 0;
    m_Stats = {};
    m_Pass = Pass::World;
}

void Renderer::EndFrame()
{
    FlushBatch(BatchBreak::Pass);
    auto now = std::chrono::steady_clock::now();
    double renderMs = std::chrono::duration<double, std::milli>(now - m_FrameStart).count();
    WriteProfileFrame(renderMs);
    if (m_ProfileFrames == 0)
    {
        m_ProfileMinDrawCalls = m_ProfileMaxDrawCalls = m_FrameDrawCalls;
    }
    else
    {
        m_ProfileMinDrawCalls = std::min(m_ProfileMinDrawCalls, m_FrameDrawCalls);
        m_ProfileMaxDrawCalls = std::max(m_ProfileMaxDrawCalls, m_FrameDrawCalls);
    }
    ++m_ProfileFrames;
    m_ProfileDrawCalls += m_FrameDrawCalls;
    double elapsed = std::chrono::duration<double>(now - m_ProfileStart).count();
    if (elapsed >= m_ProfileLogInterval && elapsed > 0)
    {
        std::ostringstream log;
        log << std::fixed << std::setprecision(1) << "[Render] frame=" << m_FrameNumber
            << " drawCalls/frame=" << m_FrameDrawCalls << " (avg=" << (double)m_ProfileDrawCalls / m_ProfileFrames
            << ", min=" << m_ProfileMinDrawCalls << ", max=" << m_ProfileMaxDrawCalls
            << ") FPS=" << m_ProfileFrames / elapsed << " samples=" << m_ProfileFrames
            << " world/post/ui=" << m_Stats.Get(RenderCounter::WorldDraws) << '/'
            << m_Stats.Get(RenderCounter::PostDraws) << '/' << m_Stats.Get(RenderCounter::UiDraws)
            << " instances=" << m_Stats.Get(RenderCounter::Instances) << " uploadKB="
            << (m_Stats.Get(RenderCounter::UploadBytes) + m_Stats.Get(RenderCounter::TextureUploadBytes)) / 1024.0
            << " build/diskHit=" << m_Stats.Get(RenderCounter::MeshBuilds) << '/'
            << m_Stats.Get(RenderCounter::DiskHits) << " renderCPUms=" << renderMs << '\n';
        std::cout << log.str();
        if (m_FrameLog.is_open())
        {
            m_FrameLog.flush();
        }
        if (m_EventLog.is_open())
        {
            m_EventLog.flush();
        }
        m_ProfileFrames = 0;
        m_ProfileDrawCalls = 0;
        m_ProfileStart = now;
    }
    m_PreviousLogMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - now).count();
}

void Renderer::SetProfilingLogInterval(double seconds)
{
    m_ProfileLogInterval = std::isfinite(seconds) ? std::max(0.0, seconds) : 1.0;
}

void Renderer::RecordDraw(bool instanced)
{
    ++m_FrameDrawCalls;
    m_Stats.Add(instanced ? RenderCounter::DrawInstanced : RenderCounter::DrawArrays);
    m_Stats.Add(
        m_Pass == Pass::World  ? RenderCounter::WorldDraws
        : m_Pass == Pass::Post ? RenderCounter::PostDraws
                               : RenderCounter::UiDraws
    );
}

void Renderer::DrawArrays(GLenum mode, GLint first, GLsizei count)
{
    glDrawArrays(mode, first, count);
    RecordDraw(false);
    m_Stats.Add(RenderCounter::UsefulVertices, count);
    m_Stats.Add(RenderCounter::SubmittedVertices, count);
}
