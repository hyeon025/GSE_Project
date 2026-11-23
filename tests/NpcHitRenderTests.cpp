// Real hidden GL regression: shared model color isolation, actor integration and cached/fallback paths.
#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include "../SimpleGame/Renderer.h"
#include "../SimpleGame/GameScene.h"
#include "GlfwTestContext.h"

namespace
{
    constexpr int Width = 900, Height = 640;
    using Pixels = std::vector<unsigned char>;

    Pixels Read()
    {
        glFinish();
        assert(glGetError() == GL_NO_ERROR);
        Pixels pixels(Width * Height * 4);
        glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    }

    Pixels DrawResident(Renderer& renderer, GameScene& scene)
    {
        renderer.BeginFrame();
        renderer.BeginWorld();
        scene.SyncCharacters();
        auto* player = scene.graph.Find(ActorKey{GameScene::PlayerNode});
        assert(player);
        player->visible = false;
        scene.RenderGraph(scene.graph);
        renderer.EndWorld();
        renderer.EndFrame();
        return Read();
    }

    size_t RedChanges(const Pixels& normal, const Pixels& hit)
    {
        size_t changed = 0;
        for (size_t i = 0; i < normal.size(); i += 4)
            if (hit[i] > hit[i + 1] * 1.6 && hit[i] > hit[i + 2] * 1.6 &&
                normal[i + 1] > hit[i + 1] + 20)
                ++changed;
        return changed;
    }

    Pixels DrawImported(Renderer& renderer, Art::DrawRequest request)
    {
        renderer.BeginFrame();
        renderer.BeginWorld();
        assert(Art::Draw(renderer, request));
        renderer.EndWorld();
        renderer.EndFrame();
        return Read();
    }

    void WriteComparison(const Pixels& normal, const Pixels& hit, const Pixels& restored)
    {
        std::ofstream out("x64/npc-hit/normal-hit-restored.ppm", std::ios::binary);
        out << "P6\n1200 640\n255\n";
        for (int y = Height - 1; y >= 0; --y)
            for (const auto* pixels : {&normal, &hit, &restored})
                for (int x = 250; x < 650; ++x)
                {
                    const size_t i = (y * Width + x) * 4;
                    out.write((const char*)pixels->data() + i, 3);
                }
        assert(out.good());
    }
}

int main()
{
    GlfwTestContext context(Width, Height, 4, "NpcHitRenderTests");
    assert(context.window);
    glewExperimental = GL_TRUE;
    assert(glewInit() == GLEW_OK);
    while (glGetError() != GL_NO_ERROR) {}
    Renderer renderer(Width, Height);
    assert(renderer.IsInitialized());
    auto settings = renderer.GetPostProcessing();
    settings.enabled = false;
    renderer.SetPostProcessing(settings);

    Game::World world;
    Society::Initialize(world);
    world.npcs.resize(1);
    world.story.childComplete = true;
    auto& npc = world.npcs[0];
    npc.artParts = Art::Assets().Recipe(0); // Armored recipe includes fixed-color metal/boots.
    npc.artPalettes = {{2, 3, 4}};
    npc.artCatalogVersion = 2;
    npc.motion.Stop();
    world.player.p = npc.p;
    assert(Society::PlayerAttack(world, npc.id));
    GameScene scene(renderer, world);
    scene.width = Width; scene.height = Height; scene.zoom = 4;
    npc.life.hitFlash = 0;
    auto normal = DrawResident(renderer, scene);
    const auto normalCalls = renderer.GetFrameDrawCalls();
    npc.life.hitFlash = Society::HitFlashDuration;
    auto hit = DrawResident(renderer, scene); // Resident GPU cache fast path.
    assert(RedChanges(normal, hit) > 10000);
    assert(renderer.GetFrameDrawCalls() == normalCalls);
    npc.life.hitFlash = 0;
    auto restored = DrawResident(renderer, scene);
    assert(normal == restored); // Colors and pixels fully restored with no shared-cache contamination.
    WriteComparison(normal, hit, restored);

    // Changing a pose with deferred animation must tint the previous resident pose immediately.
    Art::DrawRequest request;
    request.assets = npc.artParts; request.style = Art::Styles::Npc(0);
    request.x = Width / 2; request.y = 500; request.scale = 4;
    request.clip = "Walk"; request.seconds = 0; request.animationStream = 9001;
    auto oldPose = DrawImported(renderer, request);
    request.seconds = .173; request.deferAnimation = true; request.hitFlash = 1;
    auto deferredHit = DrawImported(renderer, request);
    assert(RedChanges(oldPose, deferredHit) > 10000);

    request.deferAnimation = false;
    request.yaw = .7f; request.clip = "Idle"; request.seconds = 0;
    auto coldHit = DrawImported(renderer, request); // A new mesh must receive the same tint.
    request.hitFlash = 0;
    auto coldNormal = DrawImported(renderer, request);
    assert(RedChanges(coldNormal, coldHit) > 10000);

    // Production HDR/post processing must preserve clearly visible red damage feedback too.
    settings.enabled = true;
    renderer.SetPostProcessing(settings);
    auto hdrNormal = DrawResident(renderer, scene);
    npc.life.hitFlash = Society::HitFlashDuration;
    auto hdrHit = DrawResident(renderer, scene);
    assert(RedChanges(hdrNormal, hdrHit) > 10000);
    npc.life.hitFlash = 0;
    assert(DrawResident(renderer, scene) == hdrNormal);
    settings.enabled = false;
    renderer.SetPostProcessing(settings);

    // Legacy, unpaletted Character mesh and partial tint use the same path without rebuilding.
    std::vector<RenderMeshCache::Vertex> fixed = {
        {0,0,.5f,.7f,.9f,1,0,0},{100,0,.5f,.7f,.9f,1,0,0},{0,100,.5f,.7f,.9f,1,0,0}};
    renderer.BeginFrame(); renderer.BeginWorld();
    renderer.CachedVertices(Renderer::MeshKind::Character, 982771, 20,20,1,1,fixed,false);
    assert(renderer.TryCachedMesh(Renderer::MeshKind::Character,982771,150,20,1,1,nullptr,false,1));
    assert(renderer.TryCachedMesh(Renderer::MeshKind::Character,982771,280,20,1,1,nullptr,false,.5f));
    renderer.EndWorld(); renderer.EndFrame();
    auto quads = Read();
    const size_t a=((Height-31)*Width+30)*4, b=((Height-31)*Width+160)*4,
                 c=((Height-31)*Width+290)*4;
    assert(quads[b]>quads[b+1]*2 && quads[a+1]>quads[c+1] && quads[c+1]>quads[b+1]);
    std::cout << "NPC GL hit feedback: red_pixels=" << RedChanges(normal,hit)
              << ", unchanged draw calls=" << normalCalls
              << "; restored pixels, cold/cached/deferred, HDR and fixed-color fallback passed\n";
}
