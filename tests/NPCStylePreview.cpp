// Manual visual regression sheet, using the production skinning/projection/batching.
// Run from the repository root; no game state or save is opened.
#define NOMINMAX
#include <Windows.h>
#include <fstream>
#include <stdexcept>
#include "../SimpleGame/Renderer.h"
#include "GlfwTestContext.h"
#include "../SimpleGame/ArtPresentation.h"

int main(int argc, char** argv)
{
    SetEnvironmentVariableA("LOCALAPPDATA", argc > 1 ? argv[1] : "x64/perf/npc-style-data");
    GlfwTestContext context(1440, 900, 4, "NPCStylePreview");
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
    while (glGetError() != GL_NO_ERROR)
    {
    }
    glEnable(GL_MULTISAMPLE);
    {
        Renderer renderer(1440, 900);
        if (!renderer.IsInitialized())
        {
            return 4;
        }
        const char* slots[] = {
            "Head",
            "Hair",
            "Torso",
            "Arms",
            "Legs",
            "Feet",
            "FacialHair",
            "Headgear",
            "Back",
            "Belt",
            "Mainhand",
            "Offhand"
        };
        const int parts[4][12] = {
            {1, 5, 1, 1, 1, 1, 0, 0, 3, 2, 0, 0},
            {2, 4, 6, 4, 1, 1, 0, 0, 0, 1, 0, 0},
            {1, 0, 2, 2, 3, 1, 2, 2, 0, 1, 1, 1},
            {2, 3, 3, 3, 1, 1, 0, 0, 2, 2, 2, 0}
        };
        for (int pose = 0; pose < 3; ++pose)
        {
            renderer.BeginFrame();
            renderer.BeginWorld();
            for (int row = 0; row < 2; ++row)
            {
                for (int i = 0; i < 4; ++i)
                {
                    Art::DrawRequest request;
                    for (int j = 0; j < 12; ++j)
                    {
                        if (parts[i][j])
                        {
                            request.assets.push_back(Art::Assets().Part(slots[j], parts[i][j])->id);
                        }
                    }
                    request.clip = pose == 0 ? "Idle" : pose == 1 ? "Run" : "Attack";
                    request.seconds = .2;
                    request.x = 185.f + i * 355.f;
                    request.y = 420.f + row * 440.f;
                    request.scale = 4.5f;
                    request.preciseDepth = true;
                    request.yaw = row == 0 ? Art::FacingYaw(CharacterFacing::Front) : .6f;
                    if (!Art::Draw(renderer, request))
                    {
                        throw std::runtime_error("NPC fallback");
                    }
                }
            }
            renderer.EndWorld();
            renderer.EndFrame();
            glFinish();
            if (glGetError() != GL_NO_ERROR)
            {
                return 5;
            }
            std::vector<unsigned char> pixels(1440 * 900 * 4);
            glReadPixels(0, 0, 1440, 900, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            std::ofstream output("x64/perf/npc-style-" + std::to_string(pose) + ".rgba", std::ios::binary);
            output.write((const char*)pixels.data(), pixels.size());
        }
    }
    // Context is released after renderer teardown.
}
