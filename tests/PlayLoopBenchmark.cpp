// Isolated integration benchmark: use the actual app Update, HUD, Display and swap path.
// Never call the embedded entry point/Idle/OnClose or write a game save.
#define main EmbeddedGameEntry
#include "../SimpleGame/SimpleGame.cpp"
#undef main
#include "GlfwTestContext.h"
#include <chrono>
#include <fstream>
#include <stdexcept>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        return 2;
    }
    const std::string label = argv[1], dataRoot = argv[2];
    const std::string mode = argc > 3 ? argv[3] : "idle";
    if (mode != "idle" && mode != "move" && mode != "combat")
    {
        return 6;
    }
    // The caller provides an isolated directory containing a COPY of the save.
    // Renderer logs/cache and any library state also resolve under that directory.
    SetEnvironmentVariableA("LOCALAPPDATA", dataRoot.c_str());
    const std::string save = dataRoot + "/GSE_Project/world_v1.txt";

    GlfwTestContext context(1440, 900, 4, "PlayLoopBenchmark");
    auto* window = context.window;
    if (!window)
    {
        return 1;
    }
    g_Window = window;

    struct RenderLifetime
    {
        ~RenderLifetime()
        {
            g_LevelView.reset();
            g_Scene.reset();
            g_Renderer.reset();
            g_Window = nullptr;
        }
    } renderLifetime;

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK || !GLEW_VERSION_3_3)
    {
        return 3;
    }
    glEnable(GL_MULTISAMPLE);
    // Drain GLEW's core-profile probing error before checking actual rendering.
    while (glGetError() != GL_NO_ERROR)
    {
    }

    g_SaveBlocked = true;
    g_Focused = true;
    g_Monitor.reset();
    g_Renderer.reset(new Renderer(g_Width, g_Height));
    if (!g_Renderer->IsInitialized())
    {
        return 4;
    }
    g_Renderer->SetProfilingLogInterval(1000);
    g_Scene.reset(new GameScene(*g_Renderer, g_World));
    g_Scene->width = g_Width;
    g_Scene->height = g_Height;
    bool damaged = false;
    if (!Game::Load(g_World, damaged, std::wstring(save.begin(), save.end())) || damaged)
    {
        return 5;
    }
    Society::Initialize(g_World);
    Story::PrepareMemories(g_World);
    // Migrate palettes in memory only; the harness never calls Game::Save.
    std::vector<std::vector<std::string>> originalParts;
    for (const auto& npc : g_World.npcs)
    {
        originalParts.push_back(npc.artParts);
    }
    g_Scene->SyncCharacters();
    for (size_t i = 0; i < g_World.npcs.size(); ++i)
    {
        const auto& npc = g_World.npcs[i];
        if (npc.artCatalogVersion < 2 || npc.artPalettes[2] == 0 ||
            (!originalParts[i].empty() && npc.artParts != originalParts[i]))
        {
            throw std::runtime_error("NPC appearance migration lost identity");
        }
    }
    std::vector<std::array<int, 3>> originalPalettes;
    for (const auto& npc : g_World.npcs)
    {
        originalPalettes.push_back(npc.artPalettes);
    }
    g_Scene->SyncCharacters();
    for (size_t i = 0; i < g_World.npcs.size(); ++i)
    {
        if (g_World.npcs[i].artPalettes != originalPalettes[i])
        {
            throw std::runtime_error("Unstable NPC palette");
        }
    }
    if (!Walkable(g_World.player.p))
    {
        g_World.player.p = VillageCenter(Society::NearestVillage(g_World.player.p));
    }
    if (!Walkable(g_World.player.checkpoint))
    {
        g_World.player.checkpoint = VillageCenter(Society::NearestVillage(g_World.player.checkpoint));
    }
    if (mode == "combat")
    {
        g_World.storyBattle.active = false;
        LevelOne::Start(g_World.levelOne, 710311u, false);
        auto& level = g_World.levelOne;
        level.enemies.clear();
        level.invulnerable = 100;
        for (int i = 0; i < 24; ++i)
        {
            const double radius = 72 + (i / 4) * 48;
            const int axis = i % 4;
            LevelOne::Point position(
                axis == 0   ? radius
                : axis == 1 ? -radius
                            : 0,
                axis == 2   ? radius
                : axis == 3 ? -radius
                            : 0
            );
            LevelOne::SpawnEnemy(level, i == 23 ? LevelOne::Boss : i % 3, position);
        }
    }
    g_LevelView.reset(new LevelOneView(*g_Scene, CurrentLevel()));
    RefreshTargets();
    // A fixed-step simulation makes the tested movement independent of render speed.
    // Display still executes the production HDR, full HUD, EndFrame and glfwSwapBuffers.
    std::ofstream output("x64/perf/" + label + ".txt");
    output << "mode=" << mode << " frames=180 hud=true swap=true fixed_dt=0.0166667\n";
    std::vector<double> timings, updates;
    double travelled = 0;
    for (int frame = 0; frame < 180; ++frame)
    {
        std::fill(std::begin(g_Keys), std::end(g_Keys), false);
        if (mode != "idle")
        {
            static const unsigned char keys[] = {'d', 's', 'a', 'w'};
            g_Keys[keys[(frame / 45) % 4]] = true;
        }
        const Game::Vec before = InCombat() ? Game::Vec(CurrentLevel().p.x, CurrentLevel().p.y) : g_World.player.p;
        auto begin = std::chrono::steady_clock::now();
        Update(1.f / 60);
        auto updated = std::chrono::steady_clock::now();
        const Game::Vec after = InCombat() ? Game::Vec(CurrentLevel().p.x, CurrentLevel().p.y) : g_World.player.p;
        travelled += Game::Distance(before, after);
        Display();
        glFinish(); // Only the harness waits; production Display remains asynchronous.
        timings.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
        updates.push_back(std::chrono::duration<double, std::milli>(updated - begin).count());
        if (glGetError() != GL_NO_ERROR)
        {
            throw std::runtime_error("OpenGL error in play loop");
        }
        if (frame == 30 || frame == 179)
        {
            glReadBuffer(GL_FRONT); // Display already swapped the completed frame.
            std::vector<unsigned char> pixels(g_Width * g_Height * 4);
            glReadPixels(0, 0, g_Width, g_Height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            std::ofstream image("x64/perf/" + label + "_" + std::to_string(frame) + ".rgba", std::ios::binary);
            image.write((const char*)pixels.data(), pixels.size());
            glReadBuffer(GL_BACK);
        }
    }
    output << "travelled_units=" << travelled << " combat=" << InCombat() << '\n';
    for (int skip : {0, 60})
    {
        std::vector<double> values(timings.begin() + skip, timings.end());
        double total = 0, update = 0;
        for (size_t i = skip; i < timings.size(); ++i)
        {
            total += timings[i];
            update += updates[i];
        }
        std::sort(values.begin(), values.end());
        output << "skip=" << skip << " mean_ms=" << total / values.size()
               << " p95_ms=" << values[(size_t)(values.size() * .95) - 1] << " max_ms=" << values.back()
               << " update_ms=" << update / values.size() << '\n';
    }
    // Explicit teardown while the context exists. No production close callback is registered.
    g_LevelView.reset();
    g_Scene.reset();
    g_Renderer.reset();
    // Context is released after renderer teardown.
}
