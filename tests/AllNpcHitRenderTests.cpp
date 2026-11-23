#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include "../SimpleGame/Renderer.h"
#include "../SimpleGame/LevelOneView.h"
#include "GlfwTestContext.h"

namespace
{
    constexpr int Width = 900, Height = 640;
    using Pixels = std::vector<unsigned char>;
    template<class Draw> Pixels Capture(Renderer& renderer, Draw draw)
    {
        renderer.BeginFrame(); renderer.BeginWorld();
        draw();
        renderer.EndWorld(); renderer.EndFrame();
        glFinish();
        assert(glGetError() == GL_NO_ERROR);
        Pixels pixels(Width * Height * 4);
        glReadPixels(0,0,Width,Height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        return pixels;
    }
    size_t RedChanges(const Pixels& normal, const Pixels& hit)
    {
        size_t count = 0;
        for (size_t i=0;i<normal.size();i+=4)
            if (hit[i] > hit[i+1]*1.6 && hit[i] > hit[i+2]*1.6 && normal[i+1] > hit[i+1]+15)
                ++count;
        return count;
    }
    void Restored(const Pixels& normal, const Pixels& restored)
    {
        // Cold shader/cache paths can round a handful of MSAA edge channels by 1/255.
        for(size_t i=0;i<normal.size();++i)
            assert(std::abs(int(normal[i])-int(restored[i]))<=1);
    }
    template<class Draw> void Check(Renderer& renderer, Draw draw, size_t minimum = 100)
    {
        const auto normal=Capture(renderer,[&]{draw(0);});
        const auto hit=Capture(renderer,[&]{draw(1);});
        const auto restored=Capture(renderer,[&]{draw(0);});
        assert(RedChanges(normal,hit)>minimum);
        Restored(normal,restored);
    }
}

int main()
{
    GlfwTestContext context(Width,Height,4,"AllNpcHitRenderTests");
    assert(context.window);
    glewExperimental=GL_TRUE;assert(glewInit()==GLEW_OK);
    while(glGetError()!=GL_NO_ERROR){}
    Renderer renderer(Width,Height);assert(renderer.IsInitialized());
    auto settings=renderer.GetPostProcessing();settings.enabled=false;
    renderer.SetPostProcessing(settings);
    for(unsigned recipe=0;recipe<40;++recipe)
    {
        Art::DrawRequest request;
        request.assets=Art::Assets().Recipe(recipe);
        request.style=Art::Styles::Npc(recipe+1);
        request.x=Width/2;request.y=510;request.scale=3;
        request.clip="Idle";
        Check(renderer,[&](float strength){request.hitFlash=strength;assert(Art::Draw(renderer,request));},1000);
    }
    std::vector<Pixels> monsterViews;
    for(int species=0;species<7;++species)
    {
        Art::DrawRequest request;
        request.assets={species<6?Wildlife::Asset(species):"MOB_AshenRevenant"};
        request.x=Width/2;request.y=500;request.scale=2.5f;
        request.clip="Idle";request.pixelDepth=true;
        for(float strength:{0.f,1.f,0.f})
        {
            request.hitFlash=strength;
            monsterViews.push_back(Capture(renderer,[&]{assert(Art::Draw(renderer,request));}));
        }
        const size_t at=monsterViews.size()-3;
        assert(RedChanges(monsterViews[at],monsterViews[at+1])>100);
        Restored(monsterViews[at],monsterViews[at+2]);
    }
    std::ofstream out("x64/npc-hit/all-monsters-normal-hit-restored.ppm",std::ios::binary);
    out<<"P6\n1200 2240\n255\n";
    for(int species=0;species<7;++species)
        for(int y=399;y>=80;--y)
            for(int state=0;state<3;++state)
                for(int x=250;x<650;++x)
                    out.write((const char*)monsterViews[species*3+state].data()+(y*Width+x)*4,3);
    assert(out.good());

    Game::World w;w.player.p={0,0};w.wildlifeInitialized=true;
    GameScene scene(renderer,w);scene.width=Width;scene.height=Height;scene.zoom=2;
    LevelOne::Session level;LevelOneView view(scene,level);
    for(int kind=0;kind<4;++kind)
    {
        LevelOne::Enemy enemy;enemy.id=kind+1;enemy.kind=kind;enemy.spawnTime=0;
        Check(renderer,[&](float strength){enemy.flash=strength*LevelOne::HitFlashDuration;view.Enemy(enemy);});
    }
    for(int species=0;species<6;++species)
    {
        w.wildlife.clear();
        Game::Animal animal;animal.species=species;animal.actor.id=1;
        animal.actor.p=animal.actor.home={0,0};w.wildlife.push_back(animal);
        // Use the production scene callback, including per-actor hit amount forwarding.
        Check(renderer,[&](float strength)
        {
            w.wildlife[0].hitFlash=strength*Society::HitFlashDuration;
            scene.Render(-1,-1,{});
        });
    }
    Check(renderer,[&](float strength){view.BossFallback(450,500,strength);},1000);
    // Position and scale must remain instance data when reusing the fallback mesh.
    scene.zoom=1;
    Check(renderer,[&](float strength){view.BossFallback(350,400,strength);},100);
    std::cout<<"All NPC GL feedback: 40 modular recipes, 7 monster species, 4 field roles, 6 wildlife callbacks, boss fallback position/scale and restoration (<=1/255 edge-channel rounding) passed\n";
}
