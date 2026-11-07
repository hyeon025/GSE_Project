#define NOMINMAX
#include <Windows.h>
#include "../SimpleGame/WorldActors.h"
#include <cassert>
#include <cmath>

int main()
{
    Game::World world;
    Game::Npc npc;
    npc.id = 1;
    npc.life.alive = true;
    npc.p = {10, 20};
    npc.motion.Update(1, 0, 4, 0);
    world.npcs = {npc};
    WorldNpcActor actor(
        world,
        [](const Game::Npc&, const ActorTransform&)
        {
        }
    );
    actor.data = npc;
    for (double alpha : {0., .25, .5, .75, .99})
    {
        world.society.accumulator = alpha * .1;
        actor.Synchronize();
        assert(std::abs(actor.LocalTransform().x - (6 + 4 * alpha)) < 1e-9);
        assert(std::abs(actor.data.p.x - actor.LocalTransform().x) < 1e-9);
        assert(actor.data.p.y == 20 && actor.visible);
        assert(std::abs(actor.data.motion.phase - npc.motion.phase * alpha) < 1e-9);
        assert(world.npcs[0].p.x == 10 && world.npcs[0].motion.phase == npc.motion.phase);
    }
    world.npcs[0].motion.Update(0, 0, 0, 0);
    actor.Synchronize();
    assert(!actor.data.motion.moving && actor.data.p.x == 10);
    world.npcs[0].life.alive = false;
    actor.Synchronize();
    assert(!actor.visible);
    world.npcs.clear();
    actor.Synchronize(); // A stale presentation node must not read outside the model vector.
}
