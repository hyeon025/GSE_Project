#pragma once
#include <string>
#include <vector>
#include <map>
#include "Dependencies/glew.h"
struct Color
{
    float r, g, b, a;
    Color(float red, float green, float blue, float alpha = 1) : r(red), g(green), b(blue), a(alpha) {}
};
struct PostProcessingSettings
{
    bool enabled = true;
    float exposure = 1.1f;
    float bloomStrength = 0.18f;
    float bloomThreshold = 1.0f;
    float bloomRadius = 2.5f;
    float vignetteStrength = 0.30f;
    float vignetteStart = 0.35f;
    float vignetteEnd = 1.25f;
    float edgeBlurStrength = 0.75f;
    float edgeBlurStart = 0.55f;
    float edgeBlurEnd = 1.2f;
    float edgeBlurRadius = 1.5f;
};
class Renderer
{
public:
    Renderer(int width, int height);
    ~Renderer();
    bool IsInitialized() const { return m_Program != 0 && m_Terrain != 0; }
    void Resize(int width, int height);
    void BeginWorld();
    void EndWorld();
    const PostProcessingSettings& GetPostProcessing() const { return m_PostSettings; }
    void SetPostProcessing(const PostProcessingSettings& settings);
    void Flush();
    void Triangle(float ax, float ay, float bx, float by, float cx, float cy, Color a, Color b, Color c);
    void Rect(float x, float y, float width, float height, Color color);
    void Ellipse(float x, float y, float rx, float ry, Color center, Color edge);
    void Line(float ax, float ay, float bx, float by, float width, Color color);
    void Text(float x, float y, const std::wstring& text, Color color, int size = 16);
    void Terrain(float cameraX, float cameraY, float zoom);
    void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);
    void DrawSolidRectSize(float x, float y, float z, float width, float height, float r, float g, float b, float a);
private:
    struct Vertex { float x, y, r, g, b, a, u, v; };
    struct TextTexture { GLuint id; int width, height; };
    struct RenderTarget { GLuint fbo = 0, texture = 0; };
    GLuint Program(const char* vertexPath, const char* fragmentPath);
    void Submit(const Vertex* vertices, size_t count, GLuint texture = 0);
    bool CreateTarget(RenderTarget& target, int width, int height);
    void ReleaseTargets();
    void RecreateTargets();
    void Fullscreen(GLuint program, GLuint destination, int width, int height, GLuint source);
    void Filter(GLuint source, RenderTarget* targets, bool highlights, float radius);
    int m_Width, m_Height;
    GLuint m_Program = 0, m_Terrain = 0, m_Buffer = 0, m_Array = 0;
    GLuint m_Downsample = 0, m_Blur = 0, m_Composite = 0;
    GLuint m_MsaaFbo = 0, m_MsaaColor = 0;
    RenderTarget m_Scene, m_Bloom[2], m_SoftScene[2];
    int m_FilterWidth = 1, m_FilterHeight = 1;
    bool m_TargetsReady = false, m_RenderingHdr = false;
    PostProcessingSettings m_PostSettings;
    std::vector<Vertex> m_Vertices;
    std::map<std::pair<std::wstring, int>, TextTexture> m_Text;
};
