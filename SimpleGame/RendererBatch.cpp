#include "stdafx.h"
#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include "Renderer.h"

void Renderer::InitializeBatch()
{
    m_InstancedProgram = Program("Instanced.vs", "Instanced.fs");
    m_InstanceViewport = glGetUniformLocation(m_InstancedProgram, "u_Viewport");
    m_InstanceLinearOutput = glGetUniformLocation(m_InstancedProgram, "u_LinearOutput");
    m_InstanceTexture = glGetUniformLocation(m_InstancedProgram, "u_Texture");
    m_InstanceMeshes = glGetUniformLocation(m_InstancedProgram, "u_Meshes");
    GLint texels = 0;
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &texels);
    // 128 MiB ceiling fits full cycles of the modular cast, bounded by the device limit.
    m_MeshCapacity = std::min<size_t>(4194304, (size_t)std::max(0, texels) / 2);
    m_MeshArena.Reset(m_MeshCapacity);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_MaxTextureSize);
    m_TextPageSize = std::min(2048, m_MaxTextureSize);
    glGenBuffers(1, &m_MeshBuffer);
    glBindBuffer(GL_TEXTURE_BUFFER, m_MeshBuffer);
    glBufferData(GL_TEXTURE_BUFFER, m_MeshCapacity * sizeof(Vertex), nullptr, GL_STATIC_DRAW);
    glGenTextures(1, &m_MeshTexture);
    glBindTexture(GL_TEXTURE_BUFFER, m_MeshTexture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_MeshBuffer);
    glGenVertexArrays(1, &m_InstanceArray);
    glGenBuffers(1, &m_InstanceBuffer);
    glBindVertexArray(m_InstanceArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_InstanceBuffer);
    static_assert(sizeof(Instance) == 176, "Update instance attribute layout.");
    static_assert(
        offsetof(Instance, placement) == 16 && offsetof(Instance, colors) == 32 && offsetof(Instance, options) == 80 &&
            offsetof(Instance, uv) == 96 && offsetof(Instance, palette) == 112,
        "Instance attribute offsets changed."
    );
    for (GLuint i = 0; i < 11; ++i)
    {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(
            i,
            4,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Instance),
            reinterpret_cast<void*>(static_cast<size_t>(i) * sizeof(float) * 4)
        );
        glVertexAttribDivisor(i, 1);
    }
    m_Instances.reserve(RenderBatchLimits::Instances);
}

void Renderer::ReleaseBatch()
{
    glDeleteBuffers(1, &m_InstanceBuffer);
    glDeleteBuffers(1, &m_MeshBuffer);
    glDeleteVertexArrays(1, &m_InstanceArray);
    glDeleteTextures(1, &m_MeshTexture);
    glDeleteProgram(m_InstancedProgram);
}

void Renderer::ReleaseMeshes()
{
    m_Meshes.clear();
    m_MeshRecent.clear();
    m_MeshArena.Reset(m_MeshCapacity);
    m_MeshUsed = 0;
}

Renderer::Mesh&
Renderer::GetMesh(MeshKind kind, std::uint64_t variant, const std::function<void()>& build, bool persistent)
{
    MeshKey key(kind, variant);
    auto found = m_Meshes.find(key);
    if (found != m_Meshes.end())
    {
        m_MeshRecent.splice(m_MeshRecent.end(), m_MeshRecent, found->second.recent);
        m_Stats.Add(RenderCounter::MeshHits);
        return found->second;
    }
    m_Stats.Add(RenderCounter::MeshMisses);
    std::vector<Vertex> vertices;
    if (!persistent || !LoadMeshFile(key, vertices))
    {
        auto start = std::chrono::steady_clock::now();
        m_Capture = &vertices;
        try
        {
            build();
        }
        catch (...)
        {
            m_Capture = nullptr;
            throw;
        }
        m_Capture = nullptr;
        m_Stats.Add(RenderCounter::MeshBuilds);
        auto buildUs =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        m_Stats.Add(RenderCounter::MeshBuildUs, buildUs);
        if (!RenderMeshCache::Valid(vertices))
        {
            throw std::runtime_error("Mesh builder returned invalid or oversized geometry.");
        }
        CacheEvent("build", key, vertices.size(), buildUs);
        if (persistent)
        {
            SaveMeshFile(key, vertices);
        }
    }
    if (vertices.size() > m_MeshCapacity)
    {
        throw std::runtime_error("Mesh exceeds texture buffer capacity.");
    }
    size_t offset = 0;
    while (m_Meshes.size() >= MeshCacheCapacity ||
           !m_MeshArena.Allocate(vertices.size(), offset))
    {
        // Submit all queued references BEFORE recycling any of their vertex ranges.
        // OpenGL orders the subsequent buffer update after those draws.
        FlushBatch(BatchBreak::Cache);
        if (m_MeshRecent.empty()) throw std::runtime_error("GPU mesh arena exhausted");
        auto victim = m_Meshes.find(m_MeshRecent.front());
        m_MeshArena.Release((size_t)victim->second.offset, (size_t)victim->second.count);
        m_MeshUsed -= victim->second.count;
        m_Stats.Add(RenderCounter::MeshEvictions);
        CacheEvent("mesh_evict", victim->first, victim->second.count);
        m_Meshes.erase(victim);
        m_MeshRecent.pop_front();
    }
    Mesh mesh;
    mesh.offset = (GLint)offset;
    mesh.count = (GLsizei)vertices.size();
    glBindBuffer(GL_TEXTURE_BUFFER, m_MeshBuffer);
    glBufferSubData(GL_TEXTURE_BUFFER, offset * sizeof(Vertex), vertices.size() * sizeof(Vertex), vertices.data());
    m_Stats.Add(RenderCounter::UploadBytes, vertices.size() * sizeof(Vertex));
    m_MeshUsed += vertices.size();
    m_MeshRecent.push_back(key);
    mesh.recent = std::prev(m_MeshRecent.end());
    return m_Meshes.emplace(key, mesh).first->second;
}

void Renderer::QueueInstance(const Mesh& mesh, Instance instance, GLuint texture)
{
    if (texture && m_BatchTexture && texture != m_BatchTexture)
    {
        FlushBatch(BatchBreak::Texture);
    }
    if (m_Instances.size() >= RenderBatchLimits::Instances)
    {
        FlushBatch(BatchBreak::Capacity);
    }
    GLsizei maximum = std::max(m_BatchMaxVertices, mesh.count);
    if (!m_Instances.empty() &&
        RenderBatchLimits::ExceedsPadding(m_BatchUsefulVertices + mesh.count, maximum, m_Instances.size() + 1))
    {
        FlushBatch(BatchBreak::Padding);
    }
    instance.placement[2] = (float)mesh.offset;
    instance.placement[3] = (float)mesh.count;
    if (texture)
    {
        m_BatchTexture = texture;
    }
    m_BatchMaxVertices = std::max(m_BatchMaxVertices, mesh.count);
    m_BatchUsefulVertices += mesh.count;
    m_Instances.push_back(instance);
}

void Renderer::FlushBatch(BatchBreak reason)
{
    if (m_Instances.empty())
    {
        m_Stats.Add(RenderCounter::EmptyFlushes);
        return;
    }
    const RenderCounter reasons[] = {
        RenderCounter::FlushExplicit,
        RenderCounter::FlushPass,
        RenderCounter::FlushTexture,
        RenderCounter::FlushCapacity,
        RenderCounter::FlushPadding,
        RenderCounter::FlushCache
    };
    m_Stats.Add(reasons[(size_t)reason]);
    glUseProgram(m_InstancedProgram);
    glUniform2f(m_InstanceViewport, (float)m_Width, (float)m_Height);
    glUniform1i(m_InstanceLinearOutput, m_RenderingHdr ? 1 : 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_BatchTexture);
    glUniform1i(m_InstanceTexture, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_BUFFER, m_MeshTexture);
    glUniform1i(m_InstanceMeshes, 3);
    glBindVertexArray(m_InstanceArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_InstanceBuffer);
    size_t bytes = m_Instances.size() * sizeof(Instance);
    glBufferData(GL_ARRAY_BUFFER, bytes, m_Instances.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLES, 0, m_BatchMaxVertices, (GLsizei)m_Instances.size());
    RecordDraw(true);
    m_Stats.Add(RenderCounter::UploadBytes, bytes);
    m_Stats.Add(RenderCounter::Instances, m_Instances.size());
    m_Stats.Add(RenderCounter::UsefulVertices, m_BatchUsefulVertices);
    m_Stats.Add(RenderCounter::SubmittedVertices, (std::uint64_t)m_BatchMaxVertices * m_Instances.size());
    glActiveTexture(GL_TEXTURE0);
    m_Instances.clear();
    m_BatchTexture = 0;
    m_BatchMaxVertices = 0;
    m_BatchUsefulVertices = 0;
}

void Renderer::Flush()
{
    FlushBatch(BatchBreak::Explicit);
}

bool Renderer::TryCachedMesh(MeshKind kind, std::uint64_t variant,
                             float x, float y, float scale, float opacity, const std::array<Color,4>* palette)
{
    if (m_Meshes.find(MeshKey(kind, variant)) == m_Meshes.end()) return false;
    CachedMesh(kind, variant, x, y, scale, opacity, [] {}, true, palette);
    return true;
}

void Renderer::CachedMesh(
    MeshKind kind,
    std::uint64_t variant,
    float x,
    float y,
    float scale,
    float opacity,
    const std::function<void()>& build,
    bool persistent,
    const std::array<Color,4>* palette
)
{
    if (m_Capture)
    {
        throw std::logic_error("Nested cached mesh capture is not supported.");
    }
    const auto& mesh = GetMesh(kind, variant, build, persistent);
    Instance instance;
    instance.matrix[0] = instance.matrix[3] = scale;
    instance.placement[0] = x;
    instance.placement[1] = y;
    instance.options[1] = opacity;
    if(palette) { instance.palette=*palette; instance.options[3]=1; }
    if (kind == MeshKind::Character)
    {
        instance.options[2] = 1.f;
    }
    else if (kind == MeshKind::Tree)
    {
        instance.options[2] = 3.f;
    }
    else if (kind == MeshKind::Rock || kind == MeshKind::Wall || kind == MeshKind::LevelBlock)
    {
        instance.options[2] = 2.f;
    }
    QueueInstance(mesh, instance);
}

void Renderer::Triangle(float ax, float ay, float bx, float by, float cx, float cy, Color a, Color b, Color c)
{
    if (m_Capture)
    {
        m_Capture->push_back({ax, ay, a.r, a.g, a.b, a.a, 0, 0});
        m_Capture->push_back({bx, by, b.r, b.g, b.b, b.a, 0, 0});
        m_Capture->push_back({cx, cy, c.r, c.g, c.b, c.a, 0, 0});
        return;
    }
    const auto& mesh = GetMesh(
        MeshKind::UnitTriangle,
        0,
        [this]
        {
            Triangle(0, 0, 1, 0, 0, 1, Color(1, 0, 0), Color(0, 1, 0), Color(0, 0, 1));
        }
    );
    Instance instance;
    instance.matrix[0] = bx - ax;
    instance.matrix[1] = by - ay;
    instance.matrix[2] = cx - ax;
    instance.matrix[3] = cy - ay;
    instance.placement[0] = ax;
    instance.placement[1] = ay;
    instance.colors[0] = a;
    instance.colors[1] = b;
    instance.colors[2] = c;
    instance.options[0] = 1;
    QueueInstance(mesh, instance);
}

void Renderer::Rect(float x, float y, float w, float h, Color color)
{
    if (m_Capture)
    {
        Triangle(x, y, x + w, y, x + w, y + h, color, color, color);
        Triangle(x, y, x + w, y + h, x, y + h, color, color, color);
        return;
    }
    const auto& mesh = GetMesh(
        MeshKind::UnitQuad,
        0,
        [this]
        {
            Rect(0, 0, 1, 1, Color(1, 0, 0));
        }
    );
    Instance instance;
    instance.matrix[0] = w;
    instance.matrix[3] = h;
    instance.placement[0] = x;
    instance.placement[1] = y;
    instance.colors[0] = color;
    instance.options[0] = 1;
    QueueInstance(mesh, instance);
}

void Renderer::Ellipse(float x, float y, float rx, float ry, Color center, Color edge)
{
    if (m_Capture)
    {
        for (int i = 0; i < 32; ++i)
        {
            float a = i * 6.2831853f / 32, b = (i + 1) * 6.2831853f / 32;
            Triangle(
                x,
                y,
                x + std::cos(a) * rx,
                y + std::sin(a) * ry,
                x + std::cos(b) * rx,
                y + std::sin(b) * ry,
                center,
                edge,
                edge
            );
        }
        return;
    }
    const auto& mesh = GetMesh(
        MeshKind::UnitEllipse,
        0,
        [this]
        {
            Ellipse(0, 0, 1, 1, Color(1, 0, 0), Color(0, 1, 0));
        }
    );
    Instance instance;
    instance.matrix[0] = rx;
    instance.matrix[3] = ry;
    instance.placement[0] = x;
    instance.placement[1] = y;
    instance.colors[0] = center;
    instance.colors[1] = edge;
    instance.options[0] = 1;
    QueueInstance(mesh, instance);
}

void Renderer::Line(float ax, float ay, float bx, float by, float width, Color color)
{
    float dx = bx - ax, dy = by - ay, length = std::sqrt(dx * dx + dy * dy);
    if (length < .01f)
    {
        return;
    }
    float nx = -dy / length * width * .5f, ny = dx / length * width * .5f;
    if (m_Capture)
    {
        Triangle(ax + nx, ay + ny, ax - nx, ay - ny, bx - nx, by - ny, color, color, color);
        Triangle(ax + nx, ay + ny, bx - nx, by - ny, bx + nx, by + ny, color, color, color);
        return;
    }
    const auto& mesh = GetMesh(
        MeshKind::UnitQuad,
        0,
        [this]
        {
            Rect(0, 0, 1, 1, Color(1, 0, 0));
        }
    );
    Instance instance;
    instance.matrix[0] = -2 * nx;
    instance.matrix[1] = -2 * ny;
    instance.matrix[2] = dx;
    instance.matrix[3] = dy;
    instance.placement[0] = ax + nx;
    instance.placement[1] = ay + ny;
    instance.colors[0] = color;
    instance.options[0] = 1;
    QueueInstance(mesh, instance);
}
