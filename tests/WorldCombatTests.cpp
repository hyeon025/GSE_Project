#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <iostream>
#include "../SimpleGame/WorldCombat.h"

int main()
{
    Game::World w;w.levelOne.active=false;w.player.p={0,0};
    Game::Animal wolf;wolf.actor.p={100,0};wolf.actor.home=wolf.actor.p;
    w.wildlife.push_back(wolf);
    assert(WorldCombat::Range(w)==LevelOne::Range(w.levelOne));
    WorldCombat::Update(w,.01f);
    assert(w.worldShots.size()==1 && w.worldAttackAnimation>0);
    assert(!WorldCombat::Fire(w,true));
    for(int i=0;i<30;++i)WorldCombat::Update(w,.01f,false);
    assert(w.worldShots.empty() && w.wildlife[0].health<60 && w.wildlife[0].hitFlash>0);
    w.worldShotTimer=0;w.wildlife[0].actor.p={WorldCombat::Range(w)+10,0};
    assert(!WorldCombat::Fire(w));
    w.wildlife[0].species=4;w.wildlife[0].actor.p={100,0};
    w.wildlife[0].chasing=false;
    assert(!WorldCombat::Fire(w)); // Passive animals are not attacked automatically.
    assert(WorldCombat::Fire(w,true));
    w.worldShots.clear();w.worldShotTimer=0;
    w.player.p=Game::HouseEntrance(0,0);
    auto house=Game::HousePosition(0,0);
    w.wildlife[0].species=0;w.wildlife[0].actor.p={house.x,house.y+45};
    assert(!WorldCombat::Fire(w)); // No targeting through a building.
    w.player.p={0,0};w.wildlife[0].actor.p={100,0};w.wildlife[0].health=1;
    int bread=w.player.bread;assert(WorldCombat::Fire(w));
    for(int i=0;i<30;++i)WorldCombat::Update(w,.01f,false);
    assert(w.wildlife[0].health==0 && w.wildlife[0].respawn==45 && w.player.bread==bread+1);
    for(int i=0;i<30;++i)WorldCombat::Update(w,.01f,false);
    assert(w.player.bread==bread+1);
    std::cout<<"World combat tests passed: shared range, auto/manual shots, cooldown, hit/death effects, obstruction, passive safety\n";
}
