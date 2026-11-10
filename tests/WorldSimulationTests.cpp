#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <sstream>
#include <set>
#include "../SimpleGame/WorldSimulation.h"

bool ReferenceObstacle(int tx, int ty)
{
    Game::Vec p(tx * 32.0 + 16, ty * 32.0 + 16);
    if (Game::SettlementClearing(p) || Game::Road(p) < 50 || Game::Hash(tx, ty, 37) % 100 >= 14)
        return false;
    int cx = Game::Chunk(p.x), cy = Game::Chunk(p.y);
    for (int y = cy - 1; y <= cy + 1; ++y)
        for (int x = cx - 1; x <= cx + 1; ++x)
            if (Game::Distance(Game::GetSite(x, y).p, p) < 66) return false;
    return true;
}

void CheckObstacleMemoization()
{
    for (int v = 0; v < Society::VillageCount; ++v)
    {
        auto center = Game::VillageCenter(v);
        int tx = (int)std::floor(center.x / 32), ty = (int)std::floor(center.y / 32);
        for (int y = ty - 30; y <= ty + 30; ++y)
            for (int x = tx - 30; x <= tx + 30; ++x)
            {
                bool expected = ReferenceObstacle(x, y);
                assert(Game::Obstacle(x, y) == expected);
                // Distant keys force replacements; a stale tile value must never leak.
                Game::Obstacle(x + 123456, y - 98765);
                assert(Game::Obstacle(x, y) == expected);
            }
    }
}

void CheckOrderedLocalNews()
{
    Game::World w;
    Society::Initialize(w);
    w.npcs.resize(40);
    w.society.elapsed = 1000;
    w.society.nextNews = 2000;
    w.society.letters.clear();
    for (size_t i = 0; i < w.npcs.size(); ++i)
    {
        auto& n = w.npcs[i];
        const double base = i < 20 ? -10010 : 10010;
        // Include cell boundaries, exact-distance ties and disconnected groups.
        n.p = Game::Vec(base + (i % 5) * 65, base + (i / 5 % 4) * 31);
        n.life.alive = i % 9 != 0;
        n.life.migrationCooldown = 2000;
        for (int origin = 0; origin < Society::VillageCount; ++origin)
        {
            n.life.known[origin].time = (double)((i * 17 + origin * 19) % 311);
            n.life.known[origin].stock = (int)i + origin;
        }
    }
    auto reference = w;
    for (auto& n : reference.npcs)
    {
        if (!n.life.alive) continue;
        n.life.known[n.life.village] = reference.society.villages[n.life.village].news[n.life.village];
        for (const auto& visitor : reference.npcs)
        {
            if (!visitor.life.alive || Game::Distance(n.p, visitor.p) > 65) continue;
            for (int origin = 0; origin < Society::VillageCount; ++origin)
                if (visitor.life.known[origin].time > n.life.known[origin].time)
                    n.life.known[origin] = visitor.life.known[origin];
        }
    }
    Society::ExchangeNews(w);
    for (size_t i = 0; i < w.npcs.size(); ++i)
        for (int origin = 0; origin < Society::VillageCount; ++origin)
        {
            const auto& actual = w.npcs[i].life.known[origin];
            const auto& expected = reference.npcs[i].life.known[origin];
            assert(actual.time == expected.time && actual.stock == expected.stock &&
                   actual.residents == expected.residents && actual.danger == expected.danger);
        }
}

void CheckPopulationAndRoads()
{
    Game::World w;
    Society::Initialize(w);
    assert(w.npcs.size() == Society::VillageCount * Society::ResidentsPerVillage);
    Society::Initialize(w);
    assert(w.npcs.size() == Society::VillageCount * Society::ResidentsPerVillage);
    std::set<std::wstring> names;
    std::set<std::pair<int, int>> houses;
    std::array<int, Society::VillageCount> residents = {};
    for (const auto& n : w.npcs)
    {
        assert(n.life.initialized && n.life.alive && Game::Walkable(n.p));
        assert(n.origin == 2 && n.originDeathId == 0);
        assert(!n.Name().empty() && names.insert(n.Name()).second);
        ++residents[n.life.village];
        assert(n.life.houseLot >= 0 && houses.insert({n.life.village, n.life.houseLot}).second);
        assert(!Game::Walkable(Game::HousePosition(n.life.village, n.life.houseLot)));
        assert(Game::Walkable(n.home));
        assert(Game::VillageAt(n.p) == n.life.village);
    }
    for (int v = 0; v < Society::VillageCount; ++v)
    {
        assert(residents[v] == Society::ResidentsPerVillage);
        auto c = Game::VillageCenter(v);
        for (auto goal : {Game::HuntingGround(v), Game::MeditationGround(v)})
        {
            Game::Npc traveller;
            traveller.p = c;
            for (int step = 0; step < 1000 && Game::Distance(traveller.p, goal) >= 12; ++step)
            {
                Society::Move(traveller, goal, .1);
            }
            assert(Game::Distance(traveller.p, goal) < 12);
        }
        for (int other = 0; other < Society::VillageCount; ++other)
        {
            if (other != v && Game::Distance(c, Game::VillageCenter(other)) <= Society::VillageSpacing * 1.01)
            {
                assert(std::abs(Game::Distance(c, Game::VillageCenter(other)) / Society::WalkingSpeed - 20) < .001);
                assert(Society::ClearSegment(c, Game::VillageCenter(other)));
            }
        }
        for (int b = 0; b < 3; ++b)
        {
            assert(!Game::Walkable(Game::VillageBuilding(v, b)));
        }
    }
}

void CheckSettlementMigration()
{
    Game::World w;
    Society::Initialize(w);
    w.npcs.resize(24);
    for (int i = 0; i < 24; ++i)
    {
        w.npcs[i].life.village = i / 8;
    }
    auto name = w.npcs[0].Name();
    w.npcs[0].life.alive = false;
    w.npcs[0].life.health = 0;
    w.npcs[1].life.gold = 173;
    std::stringstream oldSave("SOCIETY 2 1 30 0 0 0 0 0 0 0\n601 81 0\n602 82 0\n603 83 0\n0\n1 0\n");
    assert(Society::Read(oldSave, w.society, 24));
    assert(w.society.settledVillages == 3);
    w.player.p = Game::VillageBuilding(3, 0);
    Society::Initialize(w);
    assert(w.npcs.size() == Society::VillageCount * Society::ResidentsPerVillage && w.society.settledVillages == 7);
    assert(Game::Walkable(w.player.p));
    assert(!w.npcs[0].life.alive && w.npcs[0].Name() == name);
    assert(w.npcs[1].life.gold == 173 && w.society.villages[0].treasury == 601);
    assert(w.society.villages[4].treasury == 600);
    for (int i = 24; i < Society::VillageCount * Society::ResidentsPerVillage; ++i)
    {
        assert(w.npcs[i].id == i + 1 && w.npcs[i].life.houseLot >= 0);
    }
    std::stringstream upgraded;
    Society::Write(upgraded, w.society);
    Society::State state;
    assert(Society::Read(upgraded, state, Society::VillageCount * Society::ResidentsPerVillage));
    w.society = state;
    Society::Initialize(w);
    assert(w.npcs.size() == Society::VillageCount * Society::ResidentsPerVillage);
    auto moved = w.npcs[1];
    moved.life.village = 4;
    moved.life.job = Society::Raider;
    assert(moved.Name() == w.npcs[1].Name());
    std::stringstream lifeSave;
    Society::WriteLife(lifeSave, moved.life);
    Society::Life life;
    assert(Society::ReadLife(lifeSave, life, 40) && life.village == 4);
    life.village = Society::VillageCount;
    std::stringstream invalid;
    Society::WriteLife(invalid, life);
    assert(!Society::ReadLife(invalid, life, 40));
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
    hunter.life.routine.workRemaining = 100;
    hunter.life.routine.taskActivity = Society::Hunting;
    hunter.life.routine.taskX = hunter.p.x;
    hunter.life.routine.taskY = hunter.p.y;
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
    seeker.life.routine.workRemaining = 100;
    seeker.life.routine.leisureRemaining = 0;
    seeker.life.routine.taskActivity = Society::Studying;
    seeker.life.routine.taskX = seeker.p.x;
    seeker.life.routine.taskY = seeker.p.y;
    Society::TickNpc(w, seeker, .1, false);
    assert(seeker.life.insight == 1);
}

void CheckPlayerDeathAndEnding()
{
    Game::World w;
    w.levelOne.active = false;
    Society::Initialize(w);
    w.society.playerRaidGrace = 0;
    w.player.p = Game::HuntingGround(0);
    w.player.health = 1;
    w.npcs[4].p = w.player.p;
    w.npcs[4].life.target = -1;
    w.npcs[4].life.playerProvoked = 12;
    size_t count = w.npcs.size();
    Society::TickNpc(w, w.npcs[4], .1, true);
    assert(w.deaths == 1 && w.player.health > 0);
    assert(w.npcs.size() == count + 1);
    assert(w.npcs.back().originDeathId == 1);
    assert(w.society.playerRaidGrace == 30 && w.society.playerHitRecovery == 2);
    assert(w.npcs[4].life.target == 0);
    w.story.ending = Story::Destroy;
    Society::Die(w, w.npcs[0], 0);
    w.society.elapsed += 21;
    Society::Births(w);
    assert(w.npcs.size() == count + 1);
    assert(w.npcs[0].life.successorId == 0);
}

void CheckPlayerPursuitLimits()
{
    Game::World w;
    w.levelOne.active = false;
    w.society.initialized = true;
    w.society.settledVillages = Society::VillageCount;
    w.society.layoutVersion = 1;
    Game::Npc raider;
    raider.id = 1;
    raider.origin = 2;
    raider.archetype = 0;
    raider.home = Game::VillageCenter(0);
    raider.p = w.player.p = Game::Vec(760, 760);
    Society::InitializeNpc(raider);
    w.npcs.push_back(raider);

    Society::Decide(w, w.npcs[0], true);
    assert(w.npcs[0].life.target == 0); // New sessions and loaded games get grace.
    Society::Update(w, .1, false);
    assert(w.society.playerRaidGrace == 60);
    Society::Update(w, .1, true);
    assert(w.society.playerRaidGrace < 60);
    w.npcs[0].home = Game::VillageCenter(0);
    w.society.playerRaidGrace = 0;
    w.npcs[0].p = w.player.p;
    Society::Decide(w, w.npcs[0], true);
    assert(w.npcs[0].life.target == -1 && w.npcs[0].life.cooldown >= 1.5);
    Society::TickNpc(w, w.npcs[0], .1, true);
    assert(w.player.health == 100); // Acquiring a target is not an instant hit.

    raider.id = 2;
    w.npcs.push_back(raider);
    Society::Decide(w, w.npcs[1], true);
    assert(w.npcs[1].life.target == 0); // Only one unprovoked pursuer.
    w.npcs[0].life.cooldown = 0;
    Society::TickNpc(w, w.npcs[0], .1, true);
    float health = w.player.health;
    assert(health < 100 && health >= 94);
    w.npcs[1].life.target = -1;
    w.npcs[1].life.playerProvoked = 12;
    Society::TickNpc(w, w.npcs[1], .1, true);
    assert(w.player.health == health); // Shared recovery blocks stacked melee hits.
    w.npcs.resize(1);

    w.npcs[0].life.playerChase = 5.95;
    Society::TickNpc(w, w.npcs[0], .1, true);
    assert(w.npcs[0].life.target == 0 && w.npcs[0].life.playerRetry > 19);
    w.society.playerRaidGrace = 0;
    Society::Decide(w, w.npcs[0], true);
    assert(w.npcs[0].life.target == 0); // No immediate reacquisition after a timeout.

    w.npcs[0].life.target = -1;
    w.npcs[0].p = w.player.p = Game::VillageCenter(0);
    Society::Decide(w, w.npcs[0], true);
    assert(w.npcs[0].life.target == 0);
    w.npcs[0].life.target = -1;
    w.npcs[0].p = w.player.p = Game::Vec(1200, -1200);
    w.society.playerRaidGrace = 0;
    Society::Decide(w, w.npcs[0], true);
    assert(w.npcs[0].life.target == 0); // Home leash applies even at melee distance.

    w.npcs[0].p = Game::HuntingGround(0);
    w.player.p = Game::MeditationGround(0);
    w.npcs[0].life.target = -1;
    w.society.playerRaidGrace = 0;
    Society::Decide(w, w.npcs[0], true);
    assert(w.npcs[0].life.target == 0); // Distance also breaks pursuit before its timer expires.

    w.npcs[0].p = w.player.p = Game::VillageCenter(0);
    w.society.playerRaidGrace = 60;
    assert(Society::PlayerAttack(w, 1));
    Society::TickNpc(w, w.npcs[0], .1, true);
    assert(w.npcs[0].life.target == -1 && w.npcs[0].life.playerProvoked > 0);
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
    life.playerProvoked = 12;
    life.playerChase = 4;
    assert(Society::ReadLife(lifeStream, life, (int)w.npcs.size()));
    assert(life.job == Society::Hunter && life.village == 0);
    assert(life.playerProvoked == 0 && life.playerChase == 0);
    assert(loaded.playerRaidGrace == 60 && loaded.playerHitRecovery == 0);
    life.raidWeight = 2;
    std::stringstream bad;
    Society::WriteLife(bad, life);
    assert(!Society::ReadLife(bad, life, (int)w.npcs.size()));
    for (int i = 0; i < 200; ++i)
    {
        Society::Record(w, Society::HuntCompleted, 1);
    }
    assert(w.society.events.size() == 128);
}

void CheckLegacyRecoverySave()
{
    // NPC 13's life record from the affected SOCIETY 2 save.
    std::stringstream legacy("1 1 2 0 0 1 1 8 0 0 0 0 0 51.700065612792969 100 "
                             "0.80000001192092896 0 65.700000000000543 0\n");
    Society::Life life;
    assert(Society::ReadLife(legacy, life, 28));
    assert(life.alive && life.job == Society::Raider && life.village == 1 && life.gold == 8);
    assert(life.health == 51.700065612792969f && life.work == 0);

    life.activity = Society::Hunting;
    life.work = 65.7;
    std::stringstream interrupted;
    Society::WriteLife(interrupted, life);
    assert(Society::ReadLife(interrupted, life, 28));
    assert(life.work == Society::WorkInterval);

    life.work = 2.5;
    std::stringstream progress;
    Society::WriteLife(progress, life);
    assert(Society::ReadLife(progress, life, 28));
    assert(life.work == 2.5);

    for (const char* invalid : {"-1", "nan", "inf", "1e999"})
    {
        std::stringstream corrupt;
        corrupt << "1 1 2 0 0 1 1 8 0 0 0 0 0 50 100 0.8 0 " << invalid << " 0\n";
        assert(!Society::ReadLife(corrupt, life, 28));
    }
    std::stringstream badHealth("1 1 2 0 0 1 1 8 0 0 0 0 0 101 100 0.8 0 65.7 0\n");
    assert(!Society::ReadLife(badHealth, life, 28));
    std::stringstream truncated("1 1 2 0 0 1 1 8 0 0 0 0 0 50 100 0.8 0 65.7");
    assert(!Society::ReadLife(truncated, life, 28));

    Game::World w;
    Game::Npc npc;
    npc.id = 1;
    npc.home = npc.p = Game::VillageCenter(0);
    Society::InitializeNpc(npc);
    npc.life.work = 4;
    w.npcs.push_back(npc);
    for (int tick = 0; tick < 700; ++tick)
    {
        // Repeated injury keeps recovery active beyond the former 60-second limit.
        w.npcs[0].life.health = 25;
        Society::TickNpc(w, w.npcs[0], .1, false);
        assert(w.npcs[0].life.activity == Society::Recovering);
        assert(w.npcs[0].life.health > 25 && w.npcs[0].life.work == 0);
    }
    std::stringstream saved;
    saved << std::setprecision(17);
    Society::WriteLife(saved, w.npcs[0].life);
    assert(Society::ReadLife(saved, life, 1));
    assert(life.work == 0 && life.health == w.npcs[0].life.health);
}

void CheckMailAndMigration()
{
    Game::World w;
    Society::Initialize(w);
    w.society.elapsed = 1;
    Society::Record(w, Society::Killed, 0, 1);
    assert(w.society.letters.size() == 6);
    assert(w.society.rumors.back().reachMask == 1);
    w.society.elapsed = 20;
    Society::ExchangeNews(w);
    assert(w.society.rumors.back().reachMask == 1);
    w.society.elapsed = 100;
    Society::ExchangeNews(w);
    assert(w.society.rumors.back().reachMask == (1 << Society::VillageCount) - 1);

    for (auto& n : w.npcs)
    {
        n.life.migrationCooldown = 10000;
    }
    auto& migrant = w.npcs[0];
    migrant.life.job = Society::Hunter;
    migrant.life.raidWeight = 0;
    migrant.life.migrationCooldown = 0;
    migrant.life.target = 0;
    w.society.nextNews = 10000;
    w.society.villages[0].news[0] = {100, .9f, 20, 50};
    migrant.life.known[1] = {100, 0, 1000, 50};
    Society::ExchangeNews(w);
    assert(migrant.life.movingTo == 1 && migrant.life.village == 0);
    auto identity = migrant.Name();
    for (int tick = 0; tick < 3000 && migrant.life.movingTo >= 0; ++tick)
    {
        Society::TickNpc(w, migrant, .1, false);
    }
    assert(migrant.life.village == 1 && migrant.life.movingTo == -1);
    assert(migrant.Name() == identity && migrant.life.houseLot >= 0);
    assert(Game::Distance(migrant.home, Game::HouseEntrance(1, migrant.life.houseLot)) < .01);

    std::stringstream life;
    life << std::setprecision(17);
    Society::WriteLife(life, migrant.life, true);
    Society::Life restored;
    assert(Society::ReadLife(life, restored, Society::VillageCount * Society::ResidentsPerVillage, true));
    assert(restored.houseLot == migrant.life.houseLot && restored.known[1].time == 100);
    std::stringstream state;
    state << std::setprecision(17);
    Society::Write(state, w.society);
    Society::State loaded;
    assert(Society::Read(state, loaded, Society::VillageCount * Society::ResidentsPerVillage));
    assert(loaded.layoutVersion == 2 && loaded.rumors.back().reachMask == 127);
    assert(loaded.letters.size() == w.society.letters.size());
}

int main()
{
    CheckObstacleMemoization();
    CheckOrderedLocalNews();
    Game::World rumorWorld;
    Society::Initialize(rumorWorld);
    rumorWorld.society.elapsed = 12;
    Society::Die(rumorWorld, rumorWorld.npcs[0], 0);
    assert(rumorWorld.society.rumors.size() == 1);
    assert(rumorWorld.society.rumors[0].actor == 0 && rumorWorld.society.rumors[0].victim == 1);
    for (int i = 0; i < 140; ++i)
    {
        Society::Record(rumorWorld, Society::HuntCompleted, 2);
    }
    assert(rumorWorld.society.rumors.size() == 1);
    std::stringstream rumorSave;
    Society::Write(rumorSave, rumorWorld.society);
    Society::State loadedRumors;
    assert(Society::Read(rumorSave, loadedRumors, (int)rumorWorld.npcs.size()));
    assert(loadedRumors.rumors.size() == 1 && loadedRumors.rumors[0].time == 12);
    std::stringstream oldSociety("SOCIETY 1 1 30 0 0 0 0 0 0 0\n600 80 0\n600 80 0\n600 80 0\n0\n");
    assert(Society::Read(oldSociety, loadedRumors, 24));
    assert(loadedRumors.rumors.empty() && loadedRumors.nextRumorId == 1);
    assert(loadedRumors.settledVillages == 3);
    CheckPopulationAndRoads();
    CheckSettlementMigration();
    CheckTradeAndAttack();
    CheckOffscreenAndRecovery();
    CheckPlayerDeathAndEnding();
    CheckPlayerPursuitLimits();
    CheckSerialization();
    CheckLegacyRecoverySave();
    CheckMailAndMigration();
}
