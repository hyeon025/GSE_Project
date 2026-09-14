#include "stdafx.h"
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstring>
#include <array>
#include <stdexcept>
#include "Renderer.h"

Renderer::Renderer(int width, int height)
    : m_Width(width),
      m_Height(height)
{
    m_Program = Program("SolidRect.vs", "SolidRect.fs");
    m_Terrain = Program("Terrain.vs", "Terrain.fs");
    m_Downsample = Program("PostProcess.vs", "Downsample.fs");
    m_Blur = Program("PostProcess.vs", "Blur.fs");
    m_Composite = Program("PostProcess.vs", "PostProcess.fs");
    glGenVertexArrays(1, &m_Array);
    glGenBuffers(1, &m_Buffer);
    ConfigureVertices(m_Array, m_Buffer);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    m_Vertices.reserve(100000);
    RecreateTargets();
}

void Renderer::ConfigureVertices(GLuint array, GLuint buffer)
{
    glBindVertexArray(array);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    for (int i = 0; i < 3; ++i)
    {
        glEnableVertexAttribArray(i);
    }
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 2));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 6));
}

Renderer::~Renderer()
{
    ReleaseMeshes();
    ReleaseTargets();
    glDeleteProgram(m_Downsample);
    glDeleteProgram(m_Blur);
    glDeleteProgram(m_Composite);
    for (const auto& entry : m_Text)
    {
        glDeleteTextures(1, &entry.second.id);
    }
    glDeleteBuffers(1, &m_Buffer);
    glDeleteVertexArrays(1, &m_Array);
    glDeleteProgram(m_Program);
    glDeleteProgram(m_Terrain);
}

GLuint Renderer::Program(const char* vertexPath, const char* fragmentPath)
{
    GLuint program = glCreateProgram();
    const char* paths[] = {vertexPath, fragmentPath};
    const GLenum types[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
    for (int i = 0; i < 2; ++i)
    {
        std::string source;
        for (const auto& root : {"Shaders/", "SimpleGame/Shaders/", "../SimpleGame/Shaders/"})
        {
            std::ifstream stream(std::string(root) + paths[i], std::ios::binary);
            if (stream)
            {
                source.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
                break;
            }
        }
        if (source.empty())
        {
            wchar_t executable[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, executable, MAX_PATH);
            std::wstring folder(executable);
            folder = folder.substr(0, folder.find_last_of(L"/\\") + 1) + L"Shaders\\";
            std::string name(paths[i]);
            std::ifstream stream((folder + std::wstring(name.begin(), name.end())).c_str(), std::ios::binary);
            if (stream)
            {
                source.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
            }
        }
        if (source.empty())
        {
            std::cerr << "Missing shader: " << paths[i] << '\n';
            glDeleteProgram(program);
            return 0;
        }
        GLuint shader = glCreateShader(types[i]);
        const char* data = source.c_str();
        glShaderSource(shader, 1, &data, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[4096] = {};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << paths[i] << ": " << log << '\n';
            glDeleteShader(shader);
            glDeleteProgram(program);
            return 0;
        }
        glAttachShader(program, shader);
        glDeleteShader(shader);
    }
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[4096] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << log << '\n';
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

void Renderer::Resize(int width, int height)
{
    Flush();
    width = std::max(1, width);
    height = std::max(1, height);
    if (width != m_Width || height != m_Height)
    {
        m_Width = width;
        m_Height = height;
        RecreateTargets();
    }
    glViewport(0, 0, m_Width, m_Height);
}

bool Renderer::CreateTarget(RenderTarget& target, int width, int height)
{
    glGenTextures(1, &target.texture);
    glBindTexture(GL_TEXTURE_2D, target.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &target.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void Renderer::ReleaseTargets()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    auto release = [](RenderTarget& target)
    {
        glDeleteFramebuffers(1, &target.fbo);
        glDeleteTextures(1, &target.texture);
        target = RenderTarget();
    };
    release(m_Scene);
    for (auto& target : m_Bloom)
    {
        release(target);
    }
    for (auto& target : m_SoftScene)
    {
        release(target);
    }
    glDeleteFramebuffers(1, &m_MsaaFbo);
    glDeleteRenderbuffers(1, &m_MsaaColor);
    m_MsaaFbo = m_MsaaColor = 0;
    m_TargetsReady = m_RenderingHdr = false;
}

void Renderer::RecreateTargets()
{
    ReleaseTargets();
    if (!m_Downsample || !m_Blur || !m_Composite)
    {
        std::cerr << "Post-processing shaders unavailable; using direct rendering.\n";
        return;
    }
    glActiveTexture(GL_TEXTURE0);
    m_FilterWidth = std::max(1, (m_Width + 1) / 2);
    m_FilterHeight = std::max(1, (m_Height + 1) / 2);
    bool complete = CreateTarget(m_Scene, m_Width, m_Height);
    for (auto& target : m_Bloom)
    {
        complete = CreateTarget(target, m_FilterWidth, m_FilterHeight) && complete;
    }
    for (auto& target : m_SoftScene)
    {
        complete = CreateTarget(target, m_FilterWidth, m_FilterHeight) && complete;
    }
    if (!complete)
    {
        std::cerr << "HDR framebuffer allocation failed; using direct rendering.\n";
        ReleaseTargets();
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }

    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    if (maxSamples >= 2)
    {
        glGenFramebuffers(1, &m_MsaaFbo);
        glGenRenderbuffers(1, &m_MsaaColor);
        glBindFramebuffer(GL_FRAMEBUFFER, m_MsaaFbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_MsaaColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, std::min(4, maxSamples), GL_RGBA16F, m_Width, m_Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_MsaaColor);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            // Some devices support float targets but not multisampled float targets.
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &m_MsaaFbo);
            glDeleteRenderbuffers(1, &m_MsaaColor);
            m_MsaaFbo = m_MsaaColor = 0;
            std::cerr << "HDR multisampling unavailable; using a single-sample HDR target.\n";
        }
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_TargetsReady = true;
}

void Renderer::SetPostProcessing(const PostProcessingSettings& settings)
{
    m_PostSettings = settings;
    auto limit = [](float value, float low, float high, float fallback)
    {
        return std::isfinite(value) ? std::max(low, std::min(value, high)) : fallback;
    };
    m_PostSettings.exposure = limit(settings.exposure, 0.05f, 8.0f, 1.1f);
    m_PostSettings.bloomStrength = limit(settings.bloomStrength, 0, 2, 0.18f);
    m_PostSettings.bloomThreshold = limit(settings.bloomThreshold, 0.01f, 16, 1);
    m_PostSettings.bloomRadius = limit(settings.bloomRadius, 0.1f, 8, 2.5f);
    m_PostSettings.vignetteStrength = limit(settings.vignetteStrength, 0, 1, 0.30f);
    m_PostSettings.vignetteStart = limit(settings.vignetteStart, 0, 1.4f, 0.35f);
    m_PostSettings.vignetteEnd = limit(settings.vignetteEnd, m_PostSettings.vignetteStart + 0.01f, 2, 1.5f);
    m_PostSettings.edgeBlurStrength = limit(settings.edgeBlurStrength, 0, 1, 0.75f);
    m_PostSettings.edgeBlurStart = limit(settings.edgeBlurStart, 0, 1.4f, 0.55f);
    m_PostSettings.edgeBlurEnd = limit(settings.edgeBlurEnd, m_PostSettings.edgeBlurStart + 0.01f, 2, 1.5f);
    m_PostSettings.edgeBlurRadius = limit(settings.edgeBlurRadius, 0.1f, 8, 1.5f);
}

void Renderer::BeginWorld()
{
    Flush();
    m_RenderingHdr = m_TargetsReady;
    glBindFramebuffer(GL_FRAMEBUFFER, m_TargetsReady ? (m_MsaaFbo ? m_MsaaFbo : m_Scene.fbo) : 0);
    glViewport(0, 0, m_Width, m_Height);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::Fullscreen(GLuint program, GLuint destination, int width, int height, GLuint source)
{
    glBindFramebuffer(GL_FRAMEBUFFER, destination);
    glViewport(0, 0, width, height);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glUniform1i(glGetUniformLocation(program, "u_Source"), 0);
    glBindVertexArray(m_Array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::Filter(GLuint source, RenderTarget* targets, bool highlights, float radius)
{
    glUseProgram(m_Downsample);
    glUniform1i(glGetUniformLocation(m_Downsample, "u_Highlights"), highlights ? 1 : 0);
    glUniform1f(glGetUniformLocation(m_Downsample, "u_Threshold"), m_PostSettings.bloomThreshold);
    Fullscreen(m_Downsample, targets[0].fbo, m_FilterWidth, m_FilterHeight, source);
    // Every pair finishes in target zero; never sample the current draw attachment.
    for (int pair = 0; pair < (highlights ? 2 : 1); ++pair)
    {
        glUseProgram(m_Blur);
        glUniform2f(glGetUniformLocation(m_Blur, "u_Direction"), radius / m_FilterWidth, 0);
        Fullscreen(m_Blur, targets[1].fbo, m_FilterWidth, m_FilterHeight, targets[0].texture);
        glUniform2f(glGetUniformLocation(m_Blur, "u_Direction"), 0, radius / m_FilterHeight);
        Fullscreen(m_Blur, targets[0].fbo, m_FilterWidth, m_FilterHeight, targets[1].texture);
    }
}

void Renderer::EndWorld()
{
    Flush();
    if (!m_RenderingHdr)
    {
        return;
    }
    if (m_MsaaFbo)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_MsaaFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_Scene.fbo);
        glBlitFramebuffer(0, 0, m_Width, m_Height, 0, 0, m_Width, m_Height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glDisable(GL_BLEND);
    const auto& settings = m_PostSettings;
    bool bloom = settings.enabled && settings.bloomStrength > 0;
    bool soft = settings.enabled && settings.edgeBlurStrength > 0;
    if (bloom)
    {
        Filter(m_Scene.texture, m_Bloom, true, settings.bloomRadius);
    }
    if (soft)
    {
        Filter(m_Scene.texture, m_SoftScene, false, settings.edgeBlurRadius);
    }

    glUseProgram(m_Composite);
    glUniform1i(glGetUniformLocation(m_Composite, "u_Enabled"), settings.enabled ? 1 : 0);
    glUniform1f(glGetUniformLocation(m_Composite, "u_Exposure"), settings.exposure);
    glUniform1f(glGetUniformLocation(m_Composite, "u_BloomStrength"), settings.bloomStrength);
    glUniform3f(
        glGetUniformLocation(m_Composite, "u_Vignette"),
        settings.vignetteStrength,
        settings.vignetteStart,
        settings.vignetteEnd
    );
    glUniform3f(
        glGetUniformLocation(m_Composite, "u_EdgeBlur"),
        settings.edgeBlurStrength,
        settings.edgeBlurStart,
        settings.edgeBlurEnd
    );
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloom ? m_Bloom[0].texture : m_Scene.texture);
    glUniform1i(glGetUniformLocation(m_Composite, "u_Bloom"), 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, soft ? m_SoftScene[0].texture : m_Scene.texture);
    glUniform1i(glGetUniformLocation(m_Composite, "u_SoftScene"), 2);
    Fullscreen(m_Composite, 0, m_Width, m_Height, m_Scene.texture);

    for (int unit = 2; unit >= 0; --unit)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    m_RenderingHdr = false;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::UseGeometry(float x, float y, float scale, float opacity, GLuint texture)
{
    glUseProgram(m_Program);
    glUniform2f(glGetUniformLocation(m_Program, "u_Viewport"), (float)m_Width, (float)m_Height);
    glUniform3f(glGetUniformLocation(m_Program, "u_OffsetScale"), x, y, scale);
    glUniform1f(glGetUniformLocation(m_Program, "u_Opacity"), opacity);
    glUniform1i(glGetUniformLocation(m_Program, "u_Textured"), texture ? 1 : 0);
    glUniform1i(glGetUniformLocation(m_Program, "u_LinearOutput"), m_RenderingHdr ? 1 : 0);
    glUniform1i(glGetUniformLocation(m_Program, "u_Texture"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
}

void Renderer::Submit(const Vertex* vertices, size_t count, GLuint texture)
{
    UseGeometry(0, 0, 1, 1, texture);
    glBindVertexArray(m_Array);
    glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
    glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vertex), vertices, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)count);
}

void Renderer::ReleaseMeshes()
{
    for (const auto& entry : m_Meshes)
    {
        glDeleteBuffers(1, &entry.second.buffer);
        glDeleteVertexArrays(1, &entry.second.array);
    }
    m_Meshes.clear();
}

void Renderer::CachedMesh(
    MeshKind kind,
    std::uint64_t variant,
    float x,
    float y,
    float scale,
    float opacity,
    const std::function<void()>& build
)
{
    if (m_Capture)
    {
        throw std::logic_error("CachedMesh builders must emit primitives, not nested cached draws.");
    }

    // Preserve the painter order between dynamic batches and static meshes.
    Flush();
    auto key = std::make_pair(kind, variant);
    auto found = m_Meshes.find(key);
    if (found == m_Meshes.end())
    {
        std::vector<Vertex> vertices;
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
        if (vertices.empty())
        {
            return;
        }

        // An expanding world must not retain every visited mesh indefinitely.
        if (m_Meshes.size() >= MeshCacheCapacity)
        {
            auto oldest = std::min_element(
                m_Meshes.begin(),
                m_Meshes.end(),
                [](const auto& a, const auto& b)
                {
                    return a.second.lastUsed < b.second.lastUsed;
                }
            );
            glDeleteBuffers(1, &oldest->second.buffer);
            glDeleteVertexArrays(1, &oldest->second.array);
            m_Meshes.erase(oldest);
        }

        found = m_Meshes.emplace(key, Mesh()).first;
        auto& mesh = found->second;
        mesh.count = (GLsizei)vertices.size();
        glGenVertexArrays(1, &mesh.array);
        glGenBuffers(1, &mesh.buffer);
        ConfigureVertices(mesh.array, mesh.buffer);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    }

    auto& mesh = found->second;
    mesh.lastUsed = ++m_MeshClock;
    UseGeometry(x, y, scale, opacity);
    glBindVertexArray(mesh.array);
    glDrawArrays(GL_TRIANGLES, 0, mesh.count);
}

void Renderer::Flush()
{
    if (!m_Vertices.empty())
    {
        Submit(m_Vertices.data(), m_Vertices.size());
        m_Vertices.clear();
    }
}

void Renderer::Triangle(float ax, float ay, float bx, float by, float cx, float cy, Color a, Color b, Color c)
{
    auto& vertices = m_Capture ? *m_Capture : m_Vertices;
    vertices.push_back({ax, ay, a.r, a.g, a.b, a.a, 0, 0});
    vertices.push_back({bx, by, b.r, b.g, b.b, b.a, 0, 0});
    vertices.push_back({cx, cy, c.r, c.g, c.b, c.a, 0, 0});
}

void Renderer::Rect(float x, float y, float w, float h, Color c)
{
    Triangle(x, y, x + w, y, x + w, y + h, c, c, c);
    Triangle(x, y, x + w, y + h, x, y + h, c, c, c);
}

void Renderer::Ellipse(float x, float y, float rx, float ry, Color c, Color edge)
{
    static const auto circle = []
    {
        std::array<std::array<float, 2>, 33> points = {};
        for (int i = 0; i < 32; ++i)
        {
            float angle = i * 6.2831853f / 32;
            points[i] = {std::cos(angle), std::sin(angle)};
        }
        points[32] = points[0];
        return points;
    }();
    for (int i = 0; i < 32; ++i)
    {
        Triangle(
            x,
            y,
            x + circle[i][0] * rx,
            y + circle[i][1] * ry,
            x + circle[i + 1][0] * rx,
            y + circle[i + 1][1] * ry,
            c,
            edge,
            edge
        );
    }
}

void Renderer::Line(float ax, float ay, float bx, float by, float width, Color c)
{
    float dx = bx - ax, dy = by - ay, length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.01f)
    {
        return;
    }
    float nx = -dy / length * width * 0.5f, ny = dx / length * width * 0.5f;
    Triangle(ax + nx, ay + ny, ax - nx, ay - ny, bx - nx, by - ny, c, c, c);
    Triangle(ax + nx, ay + ny, bx - nx, by - ny, bx + nx, by + ny, c, c, c);
}

void Renderer::Text(float x, float y, const std::wstring& text, Color color, int size)
{
    if (text.empty())
    {
        return;
    }
    Flush();
    auto key = std::make_pair(text, size);
    auto found = m_Text.find(key);
    if (found == m_Text.end())
    {
        // GDI rasterizes Unicode with Korean font fallback; cache the resulting textures.
        if (m_Text.size() >= 256)
        {
            for (const auto& e : m_Text)
            {
                glDeleteTextures(1, &e.second.id);
            }
            m_Text.clear();
        }
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc)
        {
            return;
        }
        HFONT font = CreateFontW(
            -size,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,
            DEFAULT_PITCH,
            L"Malgun Gothic"
        );
        HGDIOBJ oldFont = SelectObject(dc, font);
        SIZE extent = {};
        GetTextExtentPoint32W(dc, text.c_str(), (int)text.size(), &extent);
        int w = std::max(1L, extent.cx + 4), h = std::max((LONG)size + 6, extent.cy + 4);
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        void* bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap)
        {
            SelectObject(dc, oldFont);
            DeleteObject(font);
            DeleteDC(dc);
            return;
        }
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        memset(bits, 0, (size_t)w * h * 4);
        SetBkColor(dc, RGB(0, 0, 0));
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutW(dc, 1, 1, text.c_str(), (int)text.size());
        GdiFlush();
        auto pixels = static_cast<unsigned char*>(bits);
        for (int i = 0; i < w * h; ++i)
        {
            pixels[i * 4 + 3] = pixels[i * 4];
            pixels[i * 4] = pixels[i * 4 + 1] = pixels[i * 4 + 2] = 255;
        }
        TextTexture t = {0, w, h};
        glGenTextures(1, &t.id);
        glBindTexture(GL_TEXTURE_2D, t.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bits);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        SelectObject(dc, oldBitmap);
        SelectObject(dc, oldFont);
        DeleteObject(bitmap);
        DeleteObject(font);
        DeleteDC(dc);
        found = m_Text.insert(std::make_pair(key, t)).first;
    }
    const auto& t = found->second;
    float r = color.r, g = color.g, b = color.b, a = color.a, right = x + t.width, bottom = y + t.height;
    Vertex v[] = {
        {x, y, r, g, b, a, 0, 0},
        {right, y, r, g, b, a, 1, 0},
        {right, bottom, r, g, b, a, 1, 1},
        {x, y, r, g, b, a, 0, 0},
        {right, bottom, r, g, b, a, 1, 1},
        {x, bottom, r, g, b, a, 0, 1}
    };
    Submit(v, 6, t.id);
}

void Renderer::Terrain(float cameraX, float cameraY, float zoom, bool farming, float seed)
{
    Flush();
    glUseProgram(m_Terrain);
    glUniform2f(glGetUniformLocation(m_Terrain, "u_Viewport"), (float)m_Width, (float)m_Height);
    glUniform1i(glGetUniformLocation(m_Terrain, "u_LinearOutput"), m_RenderingHdr ? 1 : 0);
    glUniform2f(glGetUniformLocation(m_Terrain, "u_Camera"), cameraX, cameraY);
    glUniform1f(glGetUniformLocation(m_Terrain, "u_Zoom"), zoom);
    glUniform1i(glGetUniformLocation(m_Terrain, "u_Farming"), farming ? 1 : 0);
    glUniform1f(glGetUniformLocation(m_Terrain, "u_Seed"), seed);
    glBindVertexArray(m_Array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a)
{
    DrawSolidRectSize(x, y, z, size, size, r, g, b, a);
}

void Renderer::DrawSolidRectSize(float x, float y, float, float w, float h, float r, float g, float b, float a)
{
    Rect(m_Width * 0.5f + x - w * 0.5f, m_Height * 0.5f - y - h * 0.5f, w, h, Color(r, g, b, a));
}
