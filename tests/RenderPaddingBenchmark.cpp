#define NOMINMAX
#include <Windows.h>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#include "../SimpleGame/Renderer.h"
#include "GlfwTestContext.h"

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        return 2;
    }
    std::string label = argv[1];
    SetEnvironmentVariableA("LOCALAPPDATA", argv[2]);
    GlfwTestContext context(1440, 900, 0, "RenderPaddingBenchmark");
    auto* window = context.window;
    if (!window)
    {
        return 1;
    }
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        return 3;
    }
    std::ofstream result("x64/perf/" + label + ".txt");
    {
        Renderer r(1440, 900);
        r.SetProfilingLogInterval(1000);
        std::vector<double> frames;
        for (int frame = 0; frame < 90; ++frame)
        {
            auto begin = std::chrono::steady_clock::now();
            r.BeginFrame();
            r.BeginWorld();
            for (int group = 0; group < 48; ++group)
            {
                float x = 25.f + (group % 12) * 115.f, y = 30.f + (group / 12) * 210.f;
                r.CachedMesh(
                    Renderer::MeshKind::Imported,
                    999001,
                    x,
                    y,
                    1,
                    1,
                    [&r]
                    {
                        for (int i = 0; i < 2000; ++i)
                        {
                            float a = (i % 40) * 1.8f, b = (i / 40) * 1.8f;
                            r.Triangle(
                                a,
                                b,
                                a + 1.5f,
                                b,
                                a,
                                b + 1.5f,
                                Color(.4f, .6f, .3f),
                                Color(.5f, .3f, .2f),
                                Color(.6f, .4f, .2f)
                            );
                        }
                    },
                    false
                );
                for (int i = 0; i < 64; ++i)
                {
                    r.Triangle(
                        x + i,
                        y + 105,
                        x + i + .5f,
                        y + 105,
                        x + i,
                        y + 125,
                        Color(.1f, .4f, .2f),
                        Color(.1f, .4f, .2f),
                        Color(.1f, .4f, .2f)
                    );
                }
            }
            r.EndWorld();
            r.EndFrame();
            glFinish();
            if (frame >= 30)
            {
                frames.push_back(
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count()
                );
            }
        }
        std::sort(frames.begin(), frames.end());
        double total = 0;
        for (auto f : frames)
        {
            total += f;
        }
        result << "mean_ms=" << total / frames.size() << " p95_ms=" << frames[(size_t)(frames.size() * .95) - 1]
               << '\n';
        std::vector<unsigned char> pixels(1440 * 900 * 4);
        glReadPixels(0, 0, 1440, 900, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        std::ofstream image("x64/perf/" + label + ".rgba", std::ios::binary);
        image.write((const char*)pixels.data(), pixels.size());
    }
    // Context is released after renderer teardown.
}
