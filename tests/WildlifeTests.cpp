#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <sstream>
#include <iostream>
#include "../SimpleGame/Wildlife.h"
#include "../SimpleGame/MonitorSnapshot.h"

int main()
{
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
