#pragma once
#include <string>
#include <vector>
#include <map>
#include <list>
#include "MeshArena.h"
#include <cstdint>
#include <functional>
#include <chrono>
#include <fstream>
#include "RenderMetrics.h"
#include "RenderMeshCache.h"
#include "Dependencies/GLEW/glew-2.2.0/include/GL/glew.h"

struct Color
{
    float r, g, b, a;

    Color(float red, float green, float blue, float alpha = 1)
        : r(red),
          g(green),
          b(blue),
          a(alpha)
    {
    }
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
    enum class MeshKind
    {
        Rock = 0,
        Tree = 1,
        Wall = 2,
        LevelBlock = 3,
        UnitTriangle = 4,
        UnitQuad = 5,
        UnitEllipse = 6,
        Character = 7,
        VillageBuilding = 8,
        Grass = 9,
        Imported = 10
    };

    Renderer(int width, int height);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool IsInitialized() const
    {
        return m_Terrain != 0 && m_InstancedProgram != 0;
    }

    bool VisibleRect(float left, float top, float right, float bottom) const
    {
        return right >= 0 && bottom >= 0 && left <= m_Width && top <= m_Height;
    }

    void Resize(int width, int height);
    void BeginFrame();
    void EndFrame();
    void SetGameFrameTiming(double updateMs, double previousSwapMs)
    {
        m_GameUpdateMs = updateMs;
        m_PreviousSwapMs = previousSwapMs;
    }

    std::uint64_t GetFrameDrawCalls() const
    {
        return m_FrameDrawCalls;
    }

    // Zero logs every frame; the default one-second window limits console overhead.
    void SetProfilingLogInterval(double seconds);

    void BeginWorld();
    void EndWorld();

    const PostProcessingSettings& GetPostProcessing() const
    {
        return m_PostSettings;
    }

    void SetPostProcessing(const PostProcessingSettings& settings);
    void Flush();
    // Queue an already resident mesh without requiring a CPU pose-cache entry.
    bool TryCachedMesh(MeshKind kind, std::uint64_t variant,
                       float x, float y, float scale, float opacity,
                       const std::array<Color,4>* palette = nullptr);
    // Queue in painter order; Flush submits compatible instances together.
    // Build local-space geometry only on a memory and disk cache miss.
    void CachedMesh(
        MeshKind kind,
        std::uint64_t variant,
        float x,
        float y,
        float scale,
        float opacity,
        const std::function<void()>& build,
        bool persistent = true,
        const std::array<Color,4>* palette = nullptr
    );
    void Triangle(float ax, float ay, float bx, float by, float cx, float cy, Color a, Color b, Color c);
    void Rect(float x, float y, float width, float height, Color color);
    void Ellipse(float x, float y, float rx, float ry, Color center, Color edge);
    void Line(float ax, float ay, float bx, float by, float width, Color color);
    void Text(float x, float y, const std::wstring& text, Color color, int size = 16);
    void Terrain(float cameraX, float cameraY, float zoom, bool farming = false, float seed = 0);
    void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);
    void DrawSolidRectSize(float x, float y, float z, float width, float height, float r, float g, float b, float a);

private:
    using Vertex = RenderMeshCache::Vertex;
    using MeshKey = std::pair<MeshKind, std::uint64_t>;

    struct TextTexture
    {
        GLuint id = 0;
        int width = 0, height = 0;
        float u = 0, v = 0, du = 1, dv = 1;
        bool standalone = false;
    };

    struct TextPage
    {
        GLuint texture = 0;
        int x = 1, y = 1, rowHeight = 0;
    };

    struct Mesh
    {
        GLint offset = 0;
        GLsizei count = 0;
        std::list<MeshKey>::iterator recent;
    };

    struct Instance
    {
        float matrix[4] = {1, 0, 0, 1};
        float placement[4] = {};
        Color colors[3] = {Color(1, 1, 1), Color(1, 1, 1), Color(1, 1, 1)};
        float options[4] = {0, 1, 0, 0};
        float uv[4] = {0, 0, 1, 1};
        std::array<Color,4> palette = {{Color(1,1,1),Color(1,1,1),Color(1,1,1),Color(1,1,1)}};
    };
    enum class Pass
    {
        World,
        Post,
        Interface
    };

    void InitializeBatch();
    void ReleaseBatch();
    Mesh& GetMesh(MeshKind kind, std::uint64_t variant, const std::function<void()>& build, bool persistent = true);
    void QueueInstance(const Mesh& mesh, Instance instance, GLuint texture = 0);
    void FlushBatch(BatchBreak reason);
    void RecordDraw(bool instanced);
    void InitializeStorage();
    bool LoadMeshFile(MeshKey key, std::vector<Vertex>& vertices);
    void SaveMeshFile(MeshKey key, const std::vector<Vertex>& vertices);
    void CacheEvent(const char* event, MeshKey key, size_t vertices = 0, std::uint64_t elapsedUs = 0);
    void OpenProfileSegment();
    void WriteProfileFrame(double renderMs);
    void ReleaseText();
    bool UploadText(TextTexture& text, const void* pixels);

    struct RenderTarget
    {
        GLuint fbo = 0, texture = 0;
    };

    GLuint Program(const char* vertexPath, const char* fragmentPath);
    void ReleaseMeshes();
    void DrawArrays(GLenum mode, GLint first, GLsizei count);
    bool CreateTarget(RenderTarget& target, int width, int height);
    void ReleaseTargets();
    void RecreateTargets();
    void Fullscreen(GLuint program, GLuint destination, int width, int height, GLuint source);
    void Filter(GLuint source, RenderTarget* targets, bool highlights, float radius);
    int m_Width, m_Height;
    GLuint m_Terrain = 0, m_Array = 0;
    GLuint m_Downsample = 0, m_Blur = 0, m_Composite = 0;
    GLuint m_MsaaFbo = 0, m_MsaaColor = 0;
    RenderTarget m_Scene, m_Bloom[2], m_SoftScene[2];
    int m_FilterWidth = 1, m_FilterHeight = 1;
    bool m_TargetsReady = false, m_RenderingHdr = false;
    PostProcessingSettings m_PostSettings;
    std::vector<Vertex>* m_Capture = nullptr;
    std::map<MeshKey, Mesh> m_Meshes;
    std::list<MeshKey> m_MeshRecent;
    MeshArena m_MeshArena;
    GLuint m_InstancedProgram = 0, m_InstanceArray = 0, m_InstanceBuffer = 0;
    GLint m_InstanceViewport = -1, m_InstanceLinearOutput = -1;
    GLint m_InstanceTexture = -1, m_InstanceMeshes = -1;
    GLuint m_MeshBuffer = 0, m_MeshTexture = 0, m_BatchTexture = 0;
    size_t m_MeshCapacity = 0, m_MeshUsed = 0;
    static constexpr size_t MeshCacheCapacity = 2048;
    std::vector<Instance> m_Instances;
    GLsizei m_BatchMaxVertices = 0;
    std::uint64_t m_BatchUsefulVertices = 0;
    std::map<std::pair<std::wstring, int>, TextTexture> m_Text;
    std::vector<TextPage> m_TextPages;
    int m_TextPageSize = 2048, m_MaxTextureSize = 2048;
    RenderFrameStats m_Stats;
    Pass m_Pass = Pass::World;
    std::wstring m_CacheDirectory, m_LogBase;
    size_t m_DiskCacheFiles = 0;
    std::ofstream m_FrameLog, m_EventLog;
    unsigned m_LogSegment = 0, m_EventSegment = 0;
    bool m_LogFailed = false;
    std::uint64_t m_ShaderFingerprint = 14695981039346656037ull;
    std::chrono::steady_clock::time_point m_FrameStart;
    std::chrono::steady_clock::time_point m_SessionStart, m_PreviousFrameStart;
    double m_FrameElapsedMs = 0, m_FrameIntervalMs = 0;
    double m_PreviousLogMs = 0;
    double m_GameUpdateMs = 0, m_PreviousSwapMs = 0;
    bool m_ProfileCombat = false;
    float m_ProfileCameraX = 0, m_ProfileCameraY = 0, m_ProfileZoom = 1, m_ProfileSeed = 0;
    std::uint64_t m_FrameNumber = 0, m_FrameDrawCalls = 0;
    std::uint64_t m_ProfileFrames = 0, m_ProfileDrawCalls = 0;
    std::uint64_t m_ProfileMinDrawCalls = 0, m_ProfileMaxDrawCalls = 0;
    std::chrono::steady_clock::time_point m_ProfileStart;
    double m_ProfileLogInterval = 1.0;
};
