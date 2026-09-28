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
            float speed = sprint ? 138.f : 85.f;
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
        const auto& p = world.player;
        if (Game::Distance(n.p, p.p) > 1100)
        {
            return;
        }
        Game::Vec goal(
            n.home.x + std::sin(world.time * .10 + n.id) * 28,
            n.home.y + std::cos(world.time * .08 + n.id) * 28
        );
        double d = Game::Distance(n.p, p.p);
        if (n.trust > 0 && (n.archetype == 1 || n.archetype == 4 || n.archetype == 6) && d > 46 && d < 250)
        {
            goal = p.p;
        }
        if (n.archetype == 2 && d < 85)
        {
            goal = Game::Vec(n.p.x + (n.p.x - p.p.x), n.p.y + (n.p.y - p.p.y));
        }
        if (n.archetype == 3)
        {
            goal = Game::Vec(
                n.home.x + std::sin(world.time * .07 + n.id) * 90,
                n.home.y + std::cos(world.time * .09 + n.id) * 90
            );
        }
        double remaining = Game::Distance(n.p, goal);
        if (remaining > 3)
        {
            double step = std::min(remaining, dt * 22.0);
            Game::Vec next(n.p.x + (goal.x - n.p.x) / remaining * step, n.p.y + (goal.y - n.p.y) / remaining * step);
            if (Game::Walkable(next))
            {
                n.p = next;
            }
        }

        data = n;
        auto pose = LocalTransform();
        pose.x = n.p.x;
        pose.y = n.p.y;
        SetLocalTransform(pose);
    }

private:
    Game::World& world;
};
