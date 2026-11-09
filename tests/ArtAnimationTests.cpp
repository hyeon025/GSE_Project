// Render a held walk/run cycle through the real asynchronous pose cache amid NPC requests.
// Read only the lower-leg pixels: changes in hair/arms cannot make a frozen gait pass.
#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>
#include "../SimpleGame/Renderer.h"
#include "../SimpleGame/ArtPresentation.h"
#include "GlfwTestContext.h"

void CheckCycle(Renderer& renderer, const char* clip, CharacterMotion& motion)
{
    Art::DrawRequest player;
    player.assets = Art::Assets().PlayerRecipe(3, 1);
    player.style = Art::Styles::Player();
    player.clip = clip;
    player.normalizedTime = true;
    player.deferAnimation = player.priorityAnimation = true;
    player.animationStream = 1;
    player.pixelDepth = true;
    player.yaw = Art::FacingYaw(CharacterFacing::Right);
    player.x = 160;
    player.y = 445;
    player.scale = 5;
    std::vector<Art::DrawRequest> crowd;
    for (unsigned i = 1; i <= 24; ++i)
    {
        auto npc = player;
        npc.assets = Art::Assets().Recipe(i);
        npc.style = Art::Styles::Npc(i);
        npc.priorityAnimation = false;
        npc.animationStream = 10 + i;
        npc.x = -500; // Exercise the worker without contaminating the player's leg pixels.
        crowd.push_back(std::move(npc));
    }
    std::set<std::uint64_t> legPoses;
    std::uint64_t last = 0;
    int still = 0, longestStill = 0;
    auto start = std::chrono::steady_clock::now();
    int wraps = 0;
    for (int frame = 0; frame < 120; ++frame)
    {
        renderer.BeginFrame();
        renderer.BeginWorld();
        for (size_t i = 0; i < crowd.size(); ++i)
        {
            crowd[i].seconds = std::fmod(frame / 60.0 + i * .037, 1.0);
            assert(Art::Draw(renderer, crowd[i]));
        }
        double previousPhase = motion.phase;
        const bool run = std::string(clip) == "Run";
        motion.Update(1, 0, (run ? 138 : 85) / 60.0, 0, run);
        assert(motion.moving && motion.sprinting == run);
        if (motion.phase < previousPhase) ++wraps;
        player.seconds = motion.ClipPhase();
        assert(Art::Draw(renderer, player));
        renderer.EndWorld();
        renderer.EndFrame();
        glFinish();
        assert(glGetError() == GL_NO_ERROR);
        std::vector<unsigned char> pixels(320 * 128 * 4);
        glReadPixels(0, 0, 320, 128, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        std::uint64_t hash = 14695981039346656037ULL;
        for (auto pixel : pixels) hash = (hash ^ pixel) * 1099511628211ULL;
        if (frame >= 15)
        {
            legPoses.insert(hash);
            still = hash == last ? still + 1 : 0;
            longestStill = std::max(longestStill, still);
        }
        last = hash;
        std::this_thread::sleep_until(start + std::chrono::microseconds((frame + 1) * 16667));
    }
    std::cout << clip << " unique_leg_frames=" << legPoses.size() << " max_unchanged_frames=" << longestStill << '\n';
    assert(legPoses.size() >= 10);
    assert(longestStill <= 8);
    assert(wraps >= 1);
    const double held = motion.phase;
    motion.Update(0, 0, 0, 0);
    assert(!motion.moving && motion.phase == held);
    player.clip = "Idle";
    player.normalizedTime = false;
    renderer.BeginFrame(); renderer.BeginWorld();
    assert(Art::Draw(renderer, player));
    renderer.EndWorld(); renderer.EndFrame();
    glFinish();
    assert(glGetError() == GL_NO_ERROR);
}

int main()
{
    GlfwTestContext context(320, 480, 4, "ArtAnimationTests");
    assert(context.window);
    glewExperimental = GL_TRUE;
    assert(glewInit() == GLEW_OK);
    while (glGetError() != GL_NO_ERROR) {}
    glEnable(GL_MULTISAMPLE);
    Renderer renderer(320, 480);
    assert(renderer.IsInitialized());
    auto post = renderer.GetPostProcessing();
    post.enabled = false;
    renderer.SetPostProcessing(post);
    CharacterMotion motion;
    CheckCycle(renderer, "Walk", motion);
    CheckCycle(renderer, "Run", motion); // Restart after a stop and change gait without resetting the cycle.
}
