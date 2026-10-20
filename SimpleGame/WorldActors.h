#pragma once
#include "Actor.h"
#include "GameWorld.h"
#include "CharacterMotion.h"

class WorldPlayerActor : public VisualActor<Game::Player>
{
public:
    WorldPlayerActor(Game::World& model, CharacterMotion& animation, Draw draw)
        : VisualActor<Game::Player>(std::move(draw)),
          world(model),
          motion(animation)
    {
    }

    Game::Vec movement;
    bool sprinting = false;

protected:
    void OnUpdate(float dt) override
    {
        auto& p = world.player;
        float x = (float)movement.x, y = (float)movement.y;
        Game::Vec previous = p.p;
        float len = std::sqrt(x * x + y * y);
        if (len > 0)
        {
            bool sprint = sprinting && p.stamina > 2;
            float speed = sprint ? 138.f : (float)Society::WalkingSpeed;
            Game::Vec before = p.p, next = p.p;
            next.x += x / len * speed * dt;
            if (Game::Walkable(next))
            {
                p.p = next;
            }
            next = p.p;
            next.y += y / len * speed * dt;
            if (Game::Walkable(next))
            {
                p.p = next;
            }
            double travel = Game::Distance(before, p.p);
            p.exploration += travel;
            while (p.exploration >= 160)
            {
                p.exploration -= 160;
                ++p.echo[3];
            }
            if (travel > 0 && sprint)
            {
                p.stamina = std::max(0.f, p.stamina - dt * 18);
            }
            if (travel > 0)
            {
                world.dirty = true;
            }
        }
        motion.Update(x, y, p.p.x - previous.x, p.p.y - previous.y);
        motion.sprinting = sprinting && motion.moving && p.stamina > 2;
        p.stamina = std::min(100.f, p.stamina + dt * 6);

        data = p;
        auto pose = LocalTransform();
        pose.x = p.p.x;
        pose.y = p.p.y;
        SetLocalTransform(pose);
    }

private:
    Game::World& world;
    CharacterMotion& motion;
};

class WorldNpcActor : public VisualActor<Game::Npc>
{
public:
    WorldNpcActor(Game::World& model, Draw draw)
        : VisualActor<Game::Npc>(std::move(draw)),
          world(model)
    {
    }

    size_t modelIndex = 0;

protected:
    void OnUpdate(float dt) override
    {
        // The index is rebound during synchronization; never retain vector element pointers.
        if (modelIndex >= world.npcs.size() || world.npcs[modelIndex].id != data.id)
        {
            return;
        }
        auto& n = world.npcs[modelIndex];
        // Simulation runs independently of visibility. This actor only synchronizes presentation.
        visible = n.life.alive;
        data = n;
        auto pose = LocalTransform();
        pose.x = n.p.x;
        pose.y = n.p.y;
        SetLocalTransform(pose);
    }

private:
    Game::World& world;
};
