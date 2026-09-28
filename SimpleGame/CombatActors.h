#pragma once
#include "Actor.h"
#include "LevelOne.h"

// Collision, damage and spawning share one ordered simulation step.
class CombatSimulationActor : public Actor
{
public:
    explicit CombatSimulationActor(LevelOne::Session& model)
        : level(model)
    {
        renderable = false;
    }

    LevelOne::Point movement;
    bool sprinting = false;

protected:
    void OnUpdate(float dt) override
    {
        LevelOne::Update(level, dt, movement, sprinting);
    }

private:
    LevelOne::Session& level;
};
