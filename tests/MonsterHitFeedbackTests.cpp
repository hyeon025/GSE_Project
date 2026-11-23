#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <iostream>
#include <sstream>
#include "../SimpleGame/WorldCombat.h"

int main()
{
    for (int species = 0; species < 6; ++species)
    {
        Game::World w;
        w.wildlifeInitialized = true;
        w.player.p = {0, 0};
        Game::Animal animal;
        animal.species = species;
        animal.actor.id = 1;
        animal.actor.p = animal.actor.home = {40, 0};
        w.wildlife.push_back(animal);
        Wildlife::InitializeStats(w.wildlife[0],w.levelOne);
        const float initialHealth=w.wildlife[0].maxHealth;
        assert(Wildlife::Attack(w));
        assert(w.wildlife[0].health == initialHealth-LevelOne::Damage(w.levelOne));
        assert(w.wildlife[0].hitFlash == Society::HitFlashDuration);
        w.wildlife[0].hitFlash = .1f;
        assert(!Wildlife::Attack(w));
        assert(w.wildlife[0].hitFlash == .1f);
        Wildlife::Update(w, .4, false);
        assert(w.wildlife[0].hitFlash == 0);
        w.wildlife[0].actor.p = w.wildlife[0].actor.home = {40, 0};
        assert(WorldCombat::Fire(w, true));
        for (int i = 0; i < 20 && !w.worldShots.empty(); ++i)
            WorldCombat::Update(w, .01f, false);
        assert(w.wildlife[0].health < initialHealth-LevelOne::Damage(w.levelOne));
        assert(w.wildlife[0].hitFlash == Society::HitFlashDuration);
    }
    for (int kind = LevelOne::Raider; kind <= LevelOne::Boss; ++kind)
    {
        LevelOne::Session s;
        s.blocked.fill(0);
        s.spawnTimer = 999;
        s.shotTimer = 999;
        LevelOne::SpawnEnemy(s, kind, {60, 0});
        s.enemies[0].spawnTime = 0;
        const float health = s.enemies[0].health;
        LevelOne::Shoot(s, s.p, s.enemies[0].p, false, 1, 260, 430);
        for (int i = 0; i < 30 && !s.projectiles.empty(); ++i)
            LevelOne::Update(s, .01f, {}, false);
        assert(s.enemies.size() == 1 && s.enemies[0].health == health - 1);
        assert(s.enemies[0].flash == LevelOne::HitFlashDuration);
        std::stringstream plain, flashed;
        LevelOne::Write(flashed, s);
        s.enemies[0].flash = 0;
        LevelOne::Write(plain, s);
        assert(flashed.str() == plain.str());
        s.enemies[0].flash = LevelOne::HitFlashDuration;
        LevelOne::Update(s, .31f, {}, false);
        assert(s.enemies[0].flash == 0);
    }
    std::cout << "Monster damage feedback: all 6 wildlife melee/projectile paths and all 4 field enemy roles, expiry and unchanged save format passed\n";
}
