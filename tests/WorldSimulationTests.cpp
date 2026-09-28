#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <sstream>
#include "../SimpleGame/WorldSimulation.h"

void CheckPopulationAndRoads()
{
    Game::World w;
    Society::Initialize(w);
    assert(w.npcs.size() == 24);
    Society::Initialize(w);
    assert(w.npcs.size() == 24);
    for (const auto& n : w.npcs)
    {
        assert(n.life.initialized && n.life.alive && Game::Walkable(n.p));
        assert(n.origin == 2 && n.originDeathId == 0);
    }
    for (int v = 0; v < 3; ++v)
    {
        auto c = Game::VillageCenter(v);
        assert(Society::ClearSegment(c, Game::HuntingGround(v)));
        assert(Society::ClearSegment(c, Game::MeditationGround(v)));
        assert(Society::ClearSegment(c, Game::VillageCenter((v + 1) % 3)));
        for (int b = 0; b < 3; ++b)
        {
            assert(!Game::Walkable(Game::VillageBuilding(v, b)));
        }
    }
}

void CheckTradeAndAttack()
{
    Game::World w;
    w.levelOne.active = false;
    Society::Initialize(w);
    auto& merchant = w.npcs[2];
    merchant.p = w.player.p = Game::VillageCenter(0);
    int total = merchant.life.gold + w.society.gold;
    int bread = w.player.bread;
    assert(Society::PlayerTrade(w, merchant, true));
    assert(w.player.bread == bread + 1);
    assert(merchant.life.gold + w.society.gold == total);
    assert(Society::PlayerTrade(w, merchant, false));
    assert(merchant.life.gold + w.society.gold == total);
    w.player.p = Game::Vec(10000, 10000);
    assert(!Society::PlayerTrade(w, merchant, true));
    assert(!Society::PlayerAttack(w, merchant.id));
    w.player.p = merchant.p;
    merchant.life.health = 20;
    int id = merchant.id;
    assert(Society::PlayerAttack(w, id));
    assert(!w.npcs[id - 1].life.alive);
    assert(!Society::PlayerAttack(w, id));
    size_t before = w.npcs.size();
    w.society.elapsed += 21;
    Society::Births(w);
    assert(w.npcs.size() == before + 1);
    assert(w.npcs.back().life.parentId == id);
    assert(w.npcs[id - 1].life.successorId == w.npcs.back().id);
    assert(w.npcs.back().echo == w.npcs[id - 1].echo);
    Society::Births(w);
    assert(w.npcs.size() == before + 1);
}

void CheckOffscreenAndRecovery()
{
    Game::World w;
    Society::Initialize(w);
    w.player.p = Game::Vec(10000, 10000);
    auto& hunter = w.npcs[0];
    hunter.p = Game::HuntingGround(0);
    hunter.life.work = 4.95;
    Society::Update(w, .1, false);
    assert(hunter.life.goods > 0);
    hunter.life.health = 15;
    hunter.p = hunter.home;
    for (int i = 0; i < 200; ++i)
    {
        Society::TickNpc(w, hunter, .1, false);
    }
    assert(hunter.life.health >= hunter.life.maxHealth * .9f);
    auto& seeker = w.npcs[5];
    seeker.p = Game::MeditationGround(0);
    seeker.life.work = 4.95;
    Society::TickNpc(w, seeker, .1, false);
    assert(seeker.life.insight == 1);
}

void CheckPlayerDeathAndEnding()
{
    Game::World w;
    w.levelOne.active = false;
    Society::Initialize(w);
    w.player.p = Game::HuntingGround(0);
    w.player.health = 1;
    w.npcs[4].p = w.player.p;
    w.npcs[4].life.target = -1;
    size_t count = w.npcs.size();
    Society::TickNpc(w, w.npcs[4], .1, true);
    assert(w.deaths == 1 && w.player.health > 0);
    assert(w.npcs.size() == count + 1);
    assert(w.npcs.back().originDeathId == 1);
    w.story.ending = Story::Destroy;
    Society::Die(w, w.npcs[0], 0);
    w.society.elapsed += 21;
    Society::Births(w);
    assert(w.npcs.size() == count + 1);
    assert(w.npcs[0].life.successorId == 0);
}

void CheckSerialization()
{
    Game::World w;
    Society::Initialize(w);
    Society::Record(w, Society::HuntCompleted, 1, 0, 2);
    std::stringstream out;
    Society::Write(out, w.society);
    Society::State loaded;
    assert(Society::Read(out, loaded, (int)w.npcs.size()));
    assert(loaded.initialized && loaded.events.size() == 1 && loaded.gold == w.society.gold);
    std::stringstream lifeStream;
    Society::WriteLife(lifeStream, w.npcs[0].life);
    Society::Life life;
    assert(Society::ReadLife(lifeStream, life, 24));
    assert(life.job == Society::Hunter && life.village == 0);
    life.raidWeight = 2;
    std::stringstream bad;
    Society::WriteLife(bad, life);
    assert(!Society::ReadLife(bad, life, 24));
    for (int i = 0; i < 200; ++i)
    {
        Society::Record(w, Society::HuntCompleted, 1);
    }
    assert(w.society.events.size() == 128);
}

int main()
{
    CheckPopulationAndRoads();
    CheckTradeAndAttack();
    CheckOffscreenAndRecovery();
    CheckPlayerDeathAndEnding();
    CheckSerialization();
}
