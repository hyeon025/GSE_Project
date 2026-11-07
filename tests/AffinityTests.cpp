#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <sstream>
#include <iostream>
#include "../SimpleGame/GameStory.h"
#include "../SimpleGame/WorldSimulation.h"
#include "../SimpleGame/MonitorSnapshot.h"

Game::World SmallWorld()
{
    Game::World w;
    w.levelOne.active = false;
    for (int id = 1; id <= 4; ++id)
    {
        Game::Npc n;
        n.id = id;
        n.origin = 2;
        n.life.village = id % 2;
        n.p = w.player.p;
        w.npcs.push_back(n);
    }
    return w;
}

int main()
{
    auto w = SmallWorld();
    auto& n = w.npcs[0];
    Story::Talk(w, n);
    assert(n.met && n.relationship == 5 && w.affinity.events.size() == 1);
    for (int i = 0; i < 10; ++i) Story::Talk(w, n);
    assert(n.relationship == 5 && w.affinity.events.size() == 1);
    Story::Talk(w, w.npcs[1]);
    assert(w.npcs[1].relationship == 5); // Cooldown is individual, not global.
    w.society.elapsed = 59.9;
    Story::Talk(w, n);
    assert(n.relationship == 5);
    w.society.elapsed = 60;
    Story::Talk(w, n);
    assert(n.relationship == 10);
    assert(Affinity::Apply(w, n, Affinity::Bread) == 25 && n.trust == 1);
    assert(Affinity::Apply(w, n, Affinity::Rescue) == 35 && n.trust == 2);
    assert(Affinity::Apply(w, n, Affinity::Support) == 15 && n.trust == 3);
    assert(Affinity::Apply(w, n, Affinity::Bread) == 15 && n.relationship == 100);
    auto count = w.affinity.events.size();
    assert(Affinity::Apply(w, n, Affinity::Training) == 0 && w.affinity.events.size() == count);
    assert(Affinity::Apply(w, n, Affinity::Threat) == -40 && n.trust == 2);
    assert(Society::PlayerAttack(w, n.id) && n.relationship == 45 && n.trust == 1);
    count = w.affinity.events.size();
    assert(!Society::PlayerAttack(w, n.id) && w.affinity.events.size() == count);
    for (int i = 0; i < 5; ++i) Affinity::Apply(w, n, Affinity::Threat);
    assert(n.relationship == -100 && n.trust == 0 && n.affinity.lastDelta == -25);
    n.life.alive = false;
    assert(Affinity::Apply(w, n, Affinity::Rescue) == 0 && n.relationship == -100);
    auto ranking = Affinity::Ranking(w);
    assert(ranking.size() == 3 && w.npcs[ranking[0]].id == 2 && w.npcs[ranking[1]].id == 3);
    assert(Affinity::Ranking(w, 0, true).size() == 1);
    assert(w.npcs[Affinity::Ranking(w, -1, false, false)[0]].id == 3);

    auto& trader = w.npcs[1];
    trader.life.job = Society::Merchant;
    trader.life.goods = 10;
    trader.life.gold = 100;
    w.society.gold = 100;
    int score = trader.relationship;
    assert(Society::PlayerTrade(w, trader, true) && trader.relationship == score + 2);
    assert(Society::PlayerTrade(w, trader, true) && trader.relationship == score + 2);
    w.society.elapsed += 30;
    assert(Society::PlayerTrade(w, trader, false) && trader.relationship == score + 4);
    w.society.gold = 0;
    count = w.affinity.events.size();
    assert(!Society::PlayerTrade(w, trader, true) && w.affinity.events.size() == count);
    for (int i = 0; i < 300; ++i)
        Affinity::Apply(w, trader, i % 2 ? Affinity::Threat : Affinity::Rescue);
    assert(w.affinity.events.size() == Affinity::HistoryLimit);
    assert(w.affinity.events.front().id > 1);

    auto ally = SmallWorld();
    ally.npcs.resize(1);
    auto& raider = ally.npcs[0];
    raider.archetype = 0;
    Society::InitializeNpc(raider);
    raider.home = Game::VillageCenter(0);
    raider.p = ally.player.p = Game::Vec(680, 0);
    ally.society.playerRaidGrace = 0;
    Affinity::Apply(ally, raider, Affinity::Bread);
    assert(raider.life.job == Society::Raider && raider.relationship == 25);
    Society::Decide(ally, raider, true);
    assert(raider.life.target == 0); // Friends do not start a raid against the player.
    assert(Society::PlayerAttack(ally, raider.id) && raider.life.target == -1); // They can still defend themselves.

    // Round-trip to an isolated, explicit path. Never open the player's real save for writing.
    Game::World saved;
    saved.levelOne.active = false;
    Society::Initialize(saved);
    Story::Talk(saved, saved.npcs[0]);
    Affinity::Apply(saved, saved.npcs[0], Affinity::Bread);
    Affinity::Apply(saved, saved.npcs[1], Affinity::Threat);
    saved.society.elapsed = 12.25;
    const std::wstring path = L"x64/affinity-tests/world.txt";
    assert(Game::Save(saved, path));
    Game::World restored;
    bool damaged = true;
    assert(Game::Load(restored, damaged, path) && !damaged);
    assert(restored.npcs[0].relationship == 30 && restored.npcs[0].trust == 1);
    assert(restored.affinity.events.size() == 3 && restored.affinity.nextId == 4);
    assert(restored.npcs[0].affinity.lastTalk == 0);
    Story::Talk(restored, restored.npcs[0]);
    assert(restored.npcs[0].relationship == 30); // Reload cannot bypass cooldown.
    assert(restored.player.profile.name == saved.player.profile.name);
    assert(restored.npcs[1].relationship == -40);

    std::ifstream input(path);
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    std::string old = text.substr(0, text.find("AFFINITY 1 "));
    old.replace(0, 11, "GSE_WORLD 7");
    { std::ofstream file("x64/affinity-tests/legacy.txt"); file << old; }
    assert(Game::Load(restored, damaged, L"x64/affinity-tests/legacy.txt") && !damaged);
    assert(restored.npcs[0].relationship == 30 && restored.npcs[0].trust == 1);
    assert(restored.npcs[0].affinity.lastTalk == -1 && restored.affinity.events.empty());
    std::string versionSix = old.substr(0, old.find("PROFILE 1 "));
    versionSix.replace(0, 11, "GSE_WORLD 6");
    { std::ofstream file("x64/affinity-tests/legacy6.txt"); file << versionSix; }
    assert(Game::Load(restored, damaged, L"x64/affinity-tests/legacy6.txt") && !damaged);
    assert(restored.npcs[0].relationship == 30 && restored.affinity.events.empty());
    // Broken extensions are rejected transactionally, leaving the current world intact.
    { std::ofstream file("x64/affinity-tests/broken.txt"); file << text.substr(0, text.find("AFFINITY 1 ")) << "AFFINITY 1 9999999 1 0\n"; }
    assert(!Game::Load(restored, damaged, L"x64/affinity-tests/broken.txt") && damaged);
    assert(restored.npcs[0].relationship == 30 && restored.npcs.size() == saved.npcs.size());
    for (const auto& corrupt : {"AFFINITY 1 4 2 129\n", "AFFINITY 2 4 2 0\n", "AFFINITY 1 4 0 0\n"})
    {
        std::stringstream bad(corrupt);
        assert(!Affinity::Read(bad, w));
        assert(w.affinity.events.size() == Affinity::HistoryLimit);
    }
    const std::string memories = "1 -1 -1 -1 0 -1\n2 -1 -1 -1 0 -1\n3 -1 -1 -1 0 -1\n4 -1 -1 -1 0 -1\n";
    for (const std::string& corrupt : {
         "AFFINITY 1 4 2 1\n" + memories + "1 5 0 0 0 5\n", // Unknown NPC.
         "AFFINITY 1 4 2 1\n" + memories + "1 1 9999 0 0 5\n", // Future time.
         "AFFINITY 1 4 2 1\n" + memories + "1 1 0 0 0 100\n", // Invalid reward.
         "AFFINITY 1 4 2 1\n" + memories + "1 1 0 99 0 5\n", // Unknown reason.
         "AFFINITY 1 4 2 1\n" + memories}) // Truncated event.
    {
        std::stringstream bad(corrupt);
        assert(!Affinity::Read(bad, w));
        assert(w.affinity.events.size() == Affinity::HistoryLimit);
    }

    auto json = Monitoring::Snapshot(saved, 9876, 1, 1, true, true);
    assert(json.find("\"relationship\":30,\"met\":true,\"trust\":1") != std::string::npos);
    assert(json.find("\"talkCooldown\":47.75") != std::string::npos);
    assert(json.find("\"before\":5,\"after\":30,\"delta\":25") != std::string::npos);
    { std::ofstream file("x64/affinity-tests/snapshot.json"); file << json; }
    std::cout << "Affinity tests passed: actions, limits, cooldowns, trade/attack, ranking, history, save migration, snapshot\n";
}
