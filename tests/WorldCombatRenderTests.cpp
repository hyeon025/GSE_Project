#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <fstream>
#include "../SimpleGame/Renderer.h"
#include "../SimpleGame/GameScene.h"
#include "GlfwTestContext.h"

int main()
{
    GlfwTestContext context(960,640,4,"WorldCombatRenderTests");assert(context.window);
    glewExperimental=GL_TRUE;assert(glewInit()==GLEW_OK);
    while(glGetError()!=GL_NO_ERROR){}
    Game::World w;w.levelOne.active=false;w.player.p={1100,0};
    Game::Animal animal;animal.actor.id=1;animal.actor.p=animal.actor.home={1200,0};w.wildlife.push_back(animal);
    Renderer renderer(960,640);assert(renderer.IsInitialized());
    GameScene scene(renderer,w);scene.width=960;scene.height=640;
    WorldCombat::Update(w,.03f);
    renderer.BeginFrame();renderer.BeginWorld();scene.Render(-1,-1,{});renderer.EndWorld();renderer.EndFrame();
    glFinish();assert(glGetError()==GL_NO_ERROR);
    assert(scene.graph.Find(ActorKey{GameScene::PlayerRangeNode}));
    assert(scene.graph.Find(ActorKey{GameScene::WorldShotNode,0}));
    assert(w.worldAttackAnimation>0 && w.worldShots.size()==1);
    std::vector<unsigned char> pixels(960*640*4);
    glReadPixels(0,0,960,640,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream out("x64/npc-navigation/world-combat-960x640.rgba",std::ios::binary);
    out.write((const char*)pixels.data(),pixels.size());
    w.worldShots.clear();renderer.BeginFrame();renderer.BeginWorld();scene.Render(-1,-1,{});renderer.EndWorld();renderer.EndFrame();
    assert(!scene.graph.Find(ActorKey{GameScene::WorldShotNode,0}));
    assert(glGetError()==GL_NO_ERROR);
}
