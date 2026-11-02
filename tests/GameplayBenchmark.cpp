// Isolated saved-world benchmark: real simulation, terrain, scenery and actors.
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include "../SimpleGame/Renderer.h"
#include "../SimpleGame/Dependencies/freeglut.h"
#include "../SimpleGame/GameScene.h"
#include "../SimpleGame/LevelOneView.h"

int main(int argc, char** argv)
{
    if (argc < 3) return 2;
    std::string label = argv[1];
    // GLUT may rewrite argc/argv; retain the requested mode before initialization.
    const std::string mode = argc > 3 ? argv[3] : "idle";
    if (mode != "idle" && mode != "move" && mode != "combat") return 6;
    SetEnvironmentVariableA("LOCALAPPDATA", argv[2]);
    Game::World world;
    bool damaged = false;
    std::string path = std::string(argv[2]) + "/GSE_Project/world_v1.txt";
    if (!Game::Load(world, damaged, std::wstring(path.begin(), path.end())) || damaged) return 5;
    Society::Initialize(world);
    const double startTime = world.time;
    glutInit(&argc, argv);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_MULTISAMPLE);
    glutSetOption(GLUT_MULTISAMPLE, 4);
    glutInitWindowSize(1440, 900);
    int window = glutCreateWindow("Saved world benchmark");
    glutHideWindow();
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return 3;
    std::ofstream result("x64/perf/" + label + ".txt");
    std::vector<double> timings, updates;
    double travelled = 0;
    {
        Renderer renderer(1440, 900);
        renderer.SetProfilingLogInterval(1000);
        GameScene scene(renderer, world);
        if (mode == "combat")
        {
            // Keep a deterministic busy encounter active throughout both timing windows.
            LevelOne::Start(world.levelOne, 710311u, false);
            auto& level = world.levelOne;
            level.enemies.clear();
            level.invulnerable = 100;
            for (int i = 0; i < 24; ++i)
            {
                const double radius = 72 + (i / 4) * 48;
                const int axis = i % 4;
                LevelOne::Point position(axis == 0 ? radius : axis == 1 ? -radius : 0,
                                         axis == 2 ? radius : axis == 3 ? -radius : 0);
                // The generated map's central cross is cleared for all seeds.
                LevelOne::SpawnEnemy(level, i == 23 ? LevelOne::Boss : i % 3, position);
            }
        }
        LevelOneView combat(scene, world.levelOne);
        result << "mode=" << mode << " frames=180";
        if (mode == "combat") result << " initial_enemies=24 temporary_invulnerability=true";
        result << '\n';
        for (int frame = 0; frame < 180; ++frame)
        {
            auto begin = std::chrono::steady_clock::now();
            world.time = startTime + frame / 60.0;
            static const IsometricView::Point directions[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
            auto input = IsometricView::GroundFromScreen(directions[(frame / 45) % 4]);
            if (mode == "combat")
            {
                scene.UpdateSociety(1.f / 60, false);
                auto before = world.levelOne.p;
                combat.Update(1.f / 60, LevelOne::Point(input.x, input.y), false);
                scene.playerMotion.Update(input.x, input.y,
                                          world.levelOne.p.x - before.x, world.levelOne.p.y - before.y);
                travelled += LevelOne::Distance(before, world.levelOne.p);
            }
            else
            {
                scene.UpdateSociety(1.f / 60, true);
                auto before = world.player.p;
                scene.Update(1.f / 60, mode == "move" ? Game::Vec(input.x, input.y) : Game::Vec(), false);
                travelled += Game::Distance(before, world.player.p);
            }
            auto updated = std::chrono::steady_clock::now();
            renderer.BeginFrame();
            renderer.BeginWorld();
            if (mode == "combat") combat.DrawWorld();
            else
            {
                auto sites = Game::Sites(world.player.p, 3);
                scene.Render(0, 0, sites);
            }
            renderer.EndWorld();
            if (mode == "combat")
                scene.DrawOverlay([&combat] { combat.DrawHud(0, 0); });
            renderer.EndFrame();
            glFinish();
            updates.push_back(std::chrono::duration<double, std::milli>(updated-begin).count());
            timings.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-begin).count());
            if (frame == 30 || frame == 179)
            {
                std::vector<unsigned char> pixels(1440*900*4);
                glReadPixels(0,0,1440,900,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                std::ofstream file("x64/perf/"+label+"_"+std::to_string(frame)+".rgba",std::ios::binary);
                file.write((const char*)pixels.data(),pixels.size());
            }
        }
        result << "travelled_units=" << travelled;
        if (mode == "combat")
            result << " remaining_enemies=" << world.levelOne.enemies.size()
                   << " final_phase=" << world.levelOne.phase;
        result << '\n';
    }
    for (int start : {0, 60})
    {
        std::vector<double> values(timings.begin()+start,timings.end());
        double total=0,update=0;
        for (size_t i=start;i<timings.size();++i) {total+=timings[i];update+=updates[i];}
        std::sort(values.begin(),values.end());
        result << "skip=" << start << " mean_ms=" << total/values.size()
               << " p95_ms=" << values[(size_t)(values.size()*.95)-1]
               << " max_ms=" << values.back() << " update_ms=" << update/values.size() << '\n';
    }
    glutDestroyWindow(window);
}
