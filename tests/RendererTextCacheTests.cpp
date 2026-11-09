#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <iostream>
#include "../SimpleGame/Renderer.h"
#include "GlfwTestContext.h"

void Draw(Renderer& renderer, const std::wstring& text, int size)
{
    renderer.BeginFrame();
    renderer.BeginWorld();
    renderer.EndWorld();
    renderer.Text(8, 8, text, Color(.8f, .9f, .7f), size);
    renderer.EndFrame();
}

std::vector<unsigned char> Pixels()
{
    glFinish();
    std::vector<unsigned char> pixels(256 * 128 * 4);
    glReadPixels(0, 0, 256, 128, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    assert(glGetError() == GL_NO_ERROR);
    return pixels;
}

int main()
{
    GlfwTestContext context(256, 128, 0, "RendererTextCacheTests");
    assert(context.window);
    glewExperimental = GL_TRUE;
    assert(glewInit() == GLEW_OK);
    while (glGetError() != GL_NO_ERROR) {}
    const DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    {
        Renderer renderer(256, 128);
        assert(renderer.IsInitialized());
        auto settings = renderer.GetPostProcessing();
        settings.enabled = false;
        renderer.SetPostProcessing(settings);
        const std::wstring sample = L"\ud638\uac10\ub3c4 +35 Walk 123";
        Draw(renderer, sample, 16);
        const auto original = Pixels();
        Draw(renderer, L"", 16);
        assert(Pixels() != original); // Exercise actual text, not just a cleared frame.
        // Exceed both font-cache (32) and text-entry (512) limits. Fonts evicted
        // after selection restoration must render identically when created again.
        for (int i = 0; i < 600; ++i)
            Draw(renderer, L"cache " + std::to_wstring(i), 8 + i % 48);
        const DWORD churned = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        if (before && churned) assert(churned <= before + 40); // 32 fonts + DC, plus driver tolerance.
        Draw(renderer, sample, 16);
        assert(Pixels() == original);
    }
    GdiFlush();
    const DWORD after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    if (before && after) assert(after <= before + 4);
    std::cout << "Renderer text cache tests passed: font/atlas churn, identical pixels, bounded handles and teardown\n";
}
