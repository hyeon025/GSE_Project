#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <iostream>
#include <limits>
#include "../SimpleGame/MonitorSnapshot.h"

class DecimalComma : public std::numpunct<char>
{
protected:
    char do_decimal_point() const override
    {
        return ',';
    }
};

int main()
{
    Monitoring::VillageTotals village;
    Society::Village market;
    assert(Monitoring::Character(village) == 0);
    village.residents = 2;
    village.raiders = 1;
    assert(Monitoring::Character(village) == 1);
    village.raiders = 0;
    village.jobs[Society::Guardian] = village.jobs[Society::Healer] = 1;
    assert(Monitoring::Character(village) == 2);
    assert(Monitoring::Mood(village, market) == 4);
    village.recentDeaths = 1;
    assert(Monitoring::Mood(village, market) == 1);
    Society::Rumor rumor;
    rumor.village = 2;
    rumor.time = 10;
    assert(Monitoring::RumorReach(rumor, 10) == 4);
    assert(Monitoring::RumorReach(rumor, 40) == 4);
    assert(Monitoring::RumorReach(rumor, 70) == 4);
    Game::World world;
    world.levelOne.active = false;
    world.time = 12.5;
    world.society.playerRaidGrace = 8.5;
    world.society.playerHitRecovery = 1.5;
    Game::Npc living;
    living.id = 1;
    living.life.village = 0;
    living.p = Game::VillageCenter(1);
    living.life.health = 25;
    living.life.gold = 17;
    world.npcs.push_back(living);
    Game::Npc dead = living;
    dead.id = 2;
    dead.life.alive = false;
    dead.life.health = 0;
    world.npcs.push_back(dead);
    rumor.id = 1;
    rumor.actor = 1;
    rumor.victim = 2;
    world.society.elapsed = 10;
    world.society.rumors.push_back(rumor);
    LevelOne::Enemy enemy;
    world.levelOne.enemies.push_back(enemy);
    auto oldLocale = std::locale();
    std::locale::global(std::locale(oldLocale, new DecimalComma));
    auto json = Monitoring::Snapshot(world, 123, 1, 2000, false);
    std::locale::global(oldLocale);
    assert(json.find("\"worldTime\":12.5") != std::string::npos);
    assert(json.find("\"raidGraceSeconds\":8.5") != std::string::npos);
    assert(json.find("\"hitRecoverySeconds\":1.5") != std::string::npos);
    assert(json.find("\"npcAlive\":1,\"npcDead\":1,\"npcTotal\":2,\"mobs\":0") != std::string::npos);
    assert(json.find("\"residents\":1,\"present\":0,\"injured\":1,\"critical\":1") != std::string::npos);
    assert(json.find("\"residents\":0,\"present\":1") != std::string::npos);
    assert(json.find("\"storedFieldAlive\":1") != std::string::npos);
    assert(json.find("\"id\":2,\"alive\":false") != std::string::npos);
    std::ostringstream expectedName;
    Monitoring::String(expectedName, living.Name());
    assert(json.find("\"name\":" + expectedName.str()) != std::string::npos);
    std::ostringstream fifthVillage;
    Monitoring::String(fifthVillage, Society::VillageName(4));
    assert(json.find("\"id\":4,\"name\":" + fifthVillage.str()) != std::string::npos);
    std::ostringstream escaped;
    Monitoring::String(escaped, L"\"\\\n\uac00");
    assert(escaped.str() == "\"\\u0022\\u005c\\u000a\\uac00\"");
    assert(json.find("\"kind\":\"murder\",\"actor\":1,\"victim\":2") != std::string::npos);
    assert(json.find("\"reachMask\":4,\"active\":true") != std::string::npos);
    world.levelOne.active = true;
    enemy.kind = LevelOne::Boss;
    enemy.spawnTime = 0;
    world.levelOne.enemies.push_back(enemy);
    enemy.health = 0;
    world.levelOne.enemies.push_back(enemy);
    json = Monitoring::Snapshot(world, 123, 2, 3000, true);
    assert(json.find("\"mobs\":2,\"spawning\":1,\"bosses\":1") != std::string::npos);
    world.storyBattle.active = true;
    json = Monitoring::Snapshot(world, 123, 3, 4000, false);
    assert(json.find("\"mobs\":0") != std::string::npos);
    world.npcs[0].p.x = std::numeric_limits<double>::quiet_NaN();
    json = Monitoring::Snapshot(world, 123, 4, 5000, true, true);
    assert(json.find("\"x\":null") != std::string::npos);
    assert(json.find("\"stopped\":true") != std::string::npos);
    world.npcs[0].life.village = 4;
    world.npcs[0].p = Game::VillageCenter(4);
    json = Monitoring::Snapshot(world, 123, 5, 6000, false);
    assert(json.find("\"village\":4,\"presentVillage\":4") != std::string::npos);
    assert(json.find("\"villageName\":" + fifthVillage.str()) != std::string::npos);
    // stdout is a complete sample message, suitable for a separate JSON-parser check.
    std::cout << json << '\n';
}
