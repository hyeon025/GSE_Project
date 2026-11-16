#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <sstream>
#include <iostream>
#include "../SimpleGame/Wildlife.h"
#include "../SimpleGame/MonitorSnapshot.h"

static void TestNpcCombat()
{
    Game::World w;
    w.player.p={2000,0};
    w.wildlifeInitialized=true;
    w.wildlife.resize(2);
    auto& wolf=w.wildlife[0];
    auto& goat=w.wildlife[1];
    goat.species=4;
    w.npcs.resize(3);
    for(int i=0;i<3;++i)w.npcs[i].id=i+1;
    auto& dead=w.npcs[0];
    dead.life.alive=false;
    dead.p={5,0};
    auto& child=w.npcs[1];
    w.story.childId=child.id;
    child.p={10,0};
    auto& resident=w.npcs[2];
    resident.p={60,0};
    resident.life.health=10;

    Wildlife::Update(w,.1,false);
    assert(wolf.chasing && wolf.actor.life.target==resident.id && wolf.actor.p.x>0);
    assert(!goat.chasing);

    const auto house=Game::HousePosition(0,0);
    const Game::Vec corner={house.x+Game::BuildingHalfWidth,house.y+Game::BuildingHalfDepth};
    wolf.actor.p=wolf.actor.home={corner.x-12,corner.y+4};
    resident.p={corner.x+4,corner.y-12};
    assert(Game::Walkable(wolf.actor.p) && Game::Walkable(resident.p));
    assert(!Society::ClearSegment(wolf.actor.p,resident.p));
    Wildlife::Update(w,.1,false);
    assert(resident.life.health==10);

    wolf.actor.home={0,0};
    wolf.actor.p={40,0};
    resident.p={60,0};
    const int gold=w.society.gold;
    Wildlife::Update(w,.1,false);
    assert(resident.life.health==5 && resident.life.alive && w.player.health==100);
    Wildlife::Update(w,.1,false);
    assert(resident.life.health==5);
    Wildlife::Update(w,1.8,false);
    assert(!resident.life.alive && w.society.deaths==1 && w.society.gold==gold);
    assert(w.society.events.back().actor==Society::WildlifeKiller);
    std::stringstream saved;
    Society::Write(saved,w.society);
    Society::State restored;
    assert(Society::Read(saved,restored,(int)w.npcs.size()));
    assert(restored.rumors.back().actor==Society::WildlifeKiller);
}

int main()
{
    TestNpcCombat();
    Game::World w;w.levelOne.active=false;Society::Initialize(w);Wildlife::Initialize(w);
    assert(w.npcs.size()==168 && w.wildlife.size()==42);
    assert(Game::HouseSpacing==160);
    for(const auto& a:w.wildlife)assert(Game::Walkable(a.actor.p));
    for(const auto& n:w.npcs)assert(Game::Walkable(n.home));
    auto& wolf=w.wildlife[0];wolf.actor.p=wolf.actor.home={0,0};
    w.player.p={60,0};
    Wildlife::Update(w,.1,true);
    assert(wolf.chasing && wolf.actor.p.x>0);
    Game::Vec toward(w.player.p.x-wolf.actor.p.x,w.player.p.y-wolf.actor.p.y);
    double d=std::hypot(toward.x,toward.y);
    assert(std::sin(wolf.yaw)*toward.x/d-std::cos(wolf.yaw)*toward.y/d>.9999);
    for(Game::Vec dir: {Game::Vec(1,0),Game::Vec(-1,0),Game::Vec(0,1),Game::Vec(0,-1),Game::Vec(1,1)})
    {
        double len=std::hypot(dir.x,dir.y);float yaw=Wildlife::Yaw(dir);
        assert(std::sin(yaw)*dir.x/len-std::cos(yaw)*dir.y/len>.9999);
    }
    auto& goat=w.wildlife[4];goat.actor.p=goat.actor.home={0,0};
    Wildlife::Update(w,.1,true);
    assert(!goat.chasing && goat.actor.p.x<0);
    wolf.actor.p={58,0};wolf.cooldown=0;w.wildlifePlayerCooldown=0;
    float hp=w.player.health;Wildlife::Update(w,.1,true);assert(w.player.health<hp);
    w.wildlifePlayerCooldown=0;assert(Wildlife::Attack(w));assert(!Wildlife::Attack(w));
    w.wildlifePlayerCooldown=0;Wildlife::Attack(w);w.wildlifePlayerCooldown=0;Wildlife::Attack(w);
    assert(wolf.health==0 && wolf.respawn>0);
    w.player.p={2000,0};Wildlife::Update(w,46,false);assert(wolf.health==60);
    assert(Monitoring::Snapshot(w,1,1,1,false).find("\"wildlifeEntities\":42")!=std::string::npos);
    // Old dense worlds keep their identities and migrate once, without adding residents.
    auto oldCount=w.npcs.size();w.society.layoutVersion=1;w.society.seeded.fill(50);
    Society::Initialize(w);assert(w.npcs.size()==oldCount && w.society.layoutVersion==2);
    std::stringstream state;Society::Write(state,w.society);Society::State restored;
    assert(Society::Read(state,restored,(int)w.npcs.size()));
    std::cout<<"Wildlife/density tests passed: 168 residents, 42 animals, heading, chase/flee, damage, attack, respawn, old density save\n";
}
