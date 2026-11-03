// Run from the repository root. Uses isolated renderer logs/cache, never game saves.
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "../SimpleGame/Renderer.h"
#include "GlfwTestContext.h"
#include "../SimpleGame/ArtPresentation.h"

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        return 2;
    }
    const std::string tag = argv[1];
    SetEnvironmentVariableA("LOCALAPPDATA", argv[2]);
    GlfwTestContext context(1440, 900, 0, "ArtRenderBenchmark");
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
    std::vector<double> cold, warm;
    std::ofstream result("x64/perf/" + tag + ".txt");
    {
        Renderer renderer(1440, 900);
        if (!renderer.IsInitialized())
        {
            return 4;
        }
        renderer.SetProfilingLogInterval(1000);
        std::vector<Art::DrawRequest> actors(48);
        for (size_t i = 0; i < actors.size(); ++i)
        {
            auto& r = actors[i];
            r.assets = Art::Assets().Recipe((unsigned)i);
            r.clip = i % 3 == 0 ? "Idle" : i % 3 == 1 ? "Walk" : "Run";
            r.yaw = Art::FacingYaw((CharacterFacing)(i % 4));
            r.x = 60.f + (i % 12) * 115.f;
            r.y = 160.f + (i / 12) * 180.f;
        }
        for (int frame = 0; frame < 240; ++frame)
        {
            auto start = std::chrono::steady_clock::now();
            renderer.BeginFrame();
            renderer.BeginWorld();
            for (auto& r : actors)
            {
                r.seconds = (frame % 120) / 30.0;
                if (!Art::Draw(renderer, r))
                {
                    throw std::runtime_error("Art fallback in benchmark");
                }
            }
            renderer.EndWorld();
            renderer.EndFrame();
            glFinish(); // Benchmark only: include completed GPU work, not just command submission.
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            (frame < 120 ? cold : warm).push_back(ms);
            if (frame == 0 || frame == 59 || frame == 119 || frame == 239)
            {
                std::vector<unsigned char> pixels(1440 * 900 * 4);
                glReadPixels(0, 0, 1440, 900, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                std::uint64_t hash = 14695981039346656037ull;
                for (auto b : pixels)
                {
                    hash = (hash ^ b) * 1099511628211ull;
                }
                result << "frame=" << frame << " pixel_hash=" << hash << '\n';
                std::ofstream image("x64/perf/" + tag + "_" + std::to_string(frame) + ".rgba", std::ios::binary);
                image.write((const char*)pixels.data(), pixels.size());
            }
        }
    }
    for (auto* values : {&cold, &warm})
    {
        double total = 0;
        for (double v : *values)
        {
            total += v;
        }
        std::sort(values->begin(), values->end());
        result << (values == &cold ? "cold" : "warm") << " mean_ms=" << total / values->size()
               << " p95_ms=" << (*values)[(size_t)(values->size() * .95) - 1] << '\n';
    }
    // Context is released after renderer teardown.
}
