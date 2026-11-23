// Combat feedback is transient and must be triggered only by successful damage.
#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include "../SimpleGame/Wildlife.h"

int main()
{
    Game::World w;
    Society::Initialize(w);
    w.npcs.resize(2);
    w.story.childComplete = true;
    auto& victim = w.npcs[0];
    auto& attacker = w.npcs[1];
    w.player.p = victim.p;
    assert(Society::PlayerAttack(w, victim.id));
    assert(victim.life.health == victim.life.maxHealth - 22);
    assert(victim.life.hitFlash == Society::HitFlashDuration);
    victim.life.hitFlash = .12f;
    const float health = victim.life.health;
    assert(!Society::PlayerAttack(w, victim.id)); // Cooldown cannot restart the flash.
    assert(victim.life.health == health && victim.life.hitFlash == .12f);
    Society::Update(w, .016, false); // No AI tick yet; the fade still advances smoothly.
    assert(std::abs(victim.life.hitFlash - .104f) < .00001f);
    assert(w.society.ticks == 0);
    Society::Update(w, 0, false);
    assert(std::abs(victim.life.hitFlash - .104f) < .00001f);
    attacker.life.cooldown = 0;
    Society::Attack(w, attacker, victim);
    assert(victim.life.health < health && victim.life.hitFlash == Society::HitFlashDuration);
    victim.life.hitFlash = .1f;
    Society::Attack(w, attacker, victim);
    assert(victim.life.hitFlash == .1f);
    Society::Update(w, .05, false);
    assert(std::abs(victim.life.hitFlash - .05f) < .00001f);
    Society::Update(w, .051, false);
    assert(victim.life.hitFlash == 0);

    victim.life.hitFlash = Society::HitFlashDuration;
    std::stringstream normal, flashed;
    auto saved = victim.life;
    saved.hitFlash = 0;
    Society::WriteLife(normal, saved, true);
    Society::WriteLife(flashed, victim.life, true);
    assert(normal.str() == flashed.str()); // Preserve the existing save format byte for byte.
    Society::Life loaded;
    loaded.hitFlash = 1;
    assert(Society::ReadLife(flashed, loaded, 2, true));
    assert(loaded.hitFlash == 0 && loaded.health == victim.life.health);

    Game::Animal wolf;
    wolf.actor.id = 1;
    wolf.actor.p = wolf.actor.home = victim.p;
    w.wildlife.push_back(wolf);
    w.wildlifeInitialized = true;
    victim.life.hitFlash = 0;
    const float beforeAnimal = victim.life.health;
    Wildlife::Update(w, .01, false);
    assert(victim.life.health == beforeAnimal - 5);
    assert(victim.life.hitFlash == Society::HitFlashDuration);
    victim.life.hitFlash = .15f;
    Wildlife::Update(w, .01, false);
    assert(victim.life.health == beforeAnimal - 5 && victim.life.hitFlash == .15f);

    victim.life.health = 1;
    attacker.life.cooldown = 0;
    Society::Attack(w, attacker, victim);
    assert(!victim.life.alive && victim.life.health == 0);
    Society::Update(w, .31, false);
    assert(victim.life.hitFlash == 0);
    assert(!Society::PlayerAttack(w, victim.id));
    std::cout << "NPC hit feedback: player/NPC/animal hits, rejected hits, frame fade, expiry, death and save reset passed\n";
}
