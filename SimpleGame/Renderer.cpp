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
#include <iomanip>
#include <sstream>
#include "Renderer.h"

Renderer::Renderer(int width, int height)
    : m_Width(width),
      m_Height(height)
{
    m_Terrain = Program("Terrain.vs", "Terrain.fs");
    m_Downsample = Program("PostProcess.vs", "Downsample.fs");
    m_Blur = Program("PostProcess.vs", "Blur.fs");
    m_Composite = Program("PostProcess.vs", "PostProcess.fs");
    glGenVertexArrays(1, &m_Array);
    InitializeBatch();
    InitializeStorage();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    RecreateTargets();
}

Renderer::~Renderer()
{
    ReleaseMeshes();
    ReleaseTargets();
    glDeleteProgram(m_Downsample);
    glDeleteProgram(m_Blur);
    glDeleteProgram(m_Composite);
    ReleaseText();
    ReleaseBatch();
    glDeleteVertexArrays(1, &m_Array);
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
        for (unsigned char value : source)
        {
            m_ShaderFingerprint = (m_ShaderFingerprint ^ value) * 1099511628211ull;
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
    FlushBatch(BatchBreak::Pass);
    m_Pass = Pass::World;
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
    DrawArrays(GL_TRIANGLES, 0, 3);
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
    FlushBatch(BatchBreak::Pass);
    if (!m_RenderingHdr)
    {
        m_Pass = Pass::Interface;
        return;
    }
    m_Pass = Pass::Post;
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
    m_Pass = Pass::Interface;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::Terrain(float cameraX, float cameraY, float zoom, bool farming, float seed)
{
    FlushBatch(BatchBreak::Pass);
    m_ProfileCombat = farming;
    m_ProfileCameraX = cameraX;
    m_ProfileCameraY = cameraY;
    m_ProfileZoom = zoom;
    m_ProfileSeed = seed;
    glUseProgram(m_Terrain);
    glUniform2f(glGetUniformLocation(m_Terrain, "u_Viewport"), (float)m_Width, (float)m_Height);
    glUniform1i(glGetUniformLocation(m_Terrain, "u_LinearOutput"), m_RenderingHdr ? 1 : 0);
    glUniform2f(glGetUniformLocation(m_Terrain, "u_Camera"), cameraX, cameraY);
    glUniform1f(glGetUniformLocation(m_Terrain, "u_Zoom"), zoom);
    glUniform1i(glGetUniformLocation(m_Terrain, "u_Farming"), farming ? 1 : 0);
    glUniform1f(glGetUniformLocation(m_Terrain, "u_Seed"), seed);
    glBindVertexArray(m_Array);
    DrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a)
{
    DrawSolidRectSize(x, y, z, size, size, r, g, b, a);
}

void Renderer::DrawSolidRectSize(float x, float y, float, float w, float h, float r, float g, float b, float a)
{
    Rect(m_Width * 0.5f + x - w * 0.5f, m_Height * 0.5f - y - h * 0.5f, w, h, Color(r, g, b, a));
}
