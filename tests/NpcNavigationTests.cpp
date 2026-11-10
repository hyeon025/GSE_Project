#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include "../SimpleGame/WorldSimulation.h"

void Arrive(Game::Vec start,Game::Vec goal,int id)
{
    Game::Npc n;n.id=id;n.p=start;n.life.activity=Society::Hunting;
    assert(Game::Walkable(start) && Game::Walkable(goal));
    int ticks=0;
    for(;ticks<2200 && Game::Distance(n.p,goal)>=12;++ticks)
    {
        auto before=n.p;
        Society::Move(n,goal,.1);
        assert(Game::Walkable(n.p));
        assert(NpcNavigation::Clear(before,n.p));
        assert(Game::Distance(before,n.p)<=4.800001);
        assert(n.life.navigation.points.size()<=96);
    }
    if(Game::Distance(n.p,goal)>=12)
    {
        std::cerr<<"Failed route "<<start.x<<','<<start.y<<" -> "<<goal.x<<','<<goal.y
                 <<" stopped "<<n.p.x<<','<<n.p.y<<" failures="<<n.life.navigation.failures<<'\n';
        std::abort();
    }
}

int main(int argc,char** argv)
{
    auto begin=std::chrono::steady_clock::now();
    // Old steering oscillates indefinitely for lots 4,24,25,26. Check every lot,
    // both work areas and the center, not merely the four observed regressions.
    for(int lot=0;lot<60;++lot)
        for(auto goal:{Game::HuntingGround(0),Game::MeditationGround(0),Game::VillageCenter(0)})
            Arrive(Game::HouseEntrance(0,lot),goal,lot+1);
    for(int v=1;v<Society::VillageCount;++v)
    {
        Arrive(Game::HouseEntrance(v,24),Game::HuntingGround(v),v+1);
        Arrive(Game::HouseEntrance(v,4),Game::MeditationGround(v),v+1);
    }
    for(int v=0;v<Society::VillageCount;++v)for(int to=0;to<Society::VillageCount;++to)
        if(v!=to)Arrive(Game::VillageCenter(v),Game::VillageCenter(to),v*7+to+1);

    Game::Npc n;n.id=1;n.p=Game::HouseEntrance(0,4);
    int budget=0;
    auto start=n.p,goal=Game::HuntingGround(0);
    Society::Move(n,goal,.1,&budget);
    assert(Game::Distance(n.p,start)==0); // Budget exhaustion waits safely without oscillating.
    budget=1;Society::Move(n,goal,.1,&budget);assert(budget==0 && Game::Distance(n.p,start)>0);
    Society::Move(n,Game::MeditationGround(0),.1);
    for(int i=0;i<2000 && Game::Distance(n.p,Game::MeditationGround(0))>=12;++i)
        Society::Move(n,Game::MeditationGround(0),.1);
    assert(Game::Distance(n.p,Game::MeditationGround(0))<12);
    auto blocked=Game::HousePosition(0,0);
    start=n.p;
    for(int i=0;i<40;++i)Society::Move(n,blocked,.1);
    assert(Game::Distance(n.p,start)==0 && n.life.navigation.failures>0);

    // Idle and stroll decisions must be individual and stable across each walk.
    Game::World w;w.levelOne.active=false;Society::Initialize(w);
    for(size_t i=0;i<w.npcs.size();++i)
    {
        auto& npc=w.npcs[i];npc.life.job=Society::Hunter;npc.life.raidWeight=0;
        npc.life.routine.workRemaining=0;npc.life.migrationCooldown=10000;
    }
    std::set<std::pair<int,int>> walks,tasks;
    int resting=0,walking=0;
    std::array<int,4> headings={};
    for(auto& npc:w.npcs)
    {
        w.society.navigationBudget=4;
        Society::TickNpc(w,npc,.1,false);
        if(npc.life.activity==Society::Resting)++resting;
        if(npc.life.activity==Society::Wandering)
        {
            ++walking;
            auto& r=npc.life.routine;
            walks.insert({(int)r.leisureX,(int)r.leisureY});
            ++headings[(int)npc.motion.facing];
            auto x=r.leisureX,y=r.leisureY;
            for(int i=0;i<5;++i)Society::TickNpc(w,npc,.1,false);
            assert(x==r.leisureX && y==r.leisureY);
        }
        npc.life.activity=Society::Hunting;
        auto p=Society::TaskPoint(npc);
        if(npc.life.village==0)tasks.insert({(int)p.x,(int)p.y});
    }
    assert(resting>40 && walking>40 && walks.size()>60 && tasks.size()>10);
    Game::Npc traveller=w.npcs[0];
    traveller.p=Game::VillageCenter(0);
    traveller.life.routine=Society::Routine();
    traveller.life.routine.workRemaining=0;
    traveller.life.navigation=Society::Navigation();
    w.society.navigationBudget=4;
    Society::TickNpc(w,traveller,.1,false);
    auto trip=Game::Vec(traveller.life.routine.taskX,traveller.life.routine.taskY);
    for(int tick=0;tick<30;++tick)
    {
        w.society.navigationBudget=4;
        Society::TickNpc(w,traveller,.1,false);
        assert(traveller.life.activity==Society::Hunting);
        assert(traveller.life.routine.taskX==trip.x && traveller.life.routine.taskY==trip.y);
    }
    // Doorways constrain the first direction. Over time, walkers should choose
    // all four headings, leave their houses, and still produce work.
    for(int tick=0;tick<600;++tick)
    {
        w.society.navigationBudget=4;
        for(auto& npc:w.npcs)
        {
            Society::TickNpc(w,npc,.1,false);
            if(npc.motion.moving) ++headings[(int)npc.motion.facing];
            assert(Game::Walkable(npc.p));
        }
    }
    for(int count:headings)assert(count>50);
    assert(w.society.trades>0);
    // Urgent recovery and migration always override leisure.
    auto& urgent=w.npcs[0];urgent.life.health=20;urgent.life.routine.leisureRemaining=20;
    Society::TickNpc(w,urgent,.1,false);
    assert(urgent.life.activity==Society::Recovering);
    urgent.life.health=urgent.life.maxHealth;urgent.life.movingTo=1;urgent.life.goal=Society::Wealth;
    Society::TickNpc(w,urgent,.1,false);
    assert(urgent.life.activity==Society::Travelling);

    // A saved resting activity is readable; session-local paths/timers are rebuilt.
    std::stringstream saved;
    w.npcs[1].life.activity=Society::Wandering;
    Society::WriteLife(saved,w.npcs[1].life,true);
    Society::Life restored;
    assert(Society::ReadLife(saved,restored,(int)w.npcs.size(),true));
    assert(restored.activity==Society::Wandering && restored.navigation.points.empty() && restored.routine.workRemaining<0);
    assert(Game::Save(w,L"x64/npc-navigation/world9.txt"));
    Game::World loaded;bool damaged=true;
    assert(Game::Load(loaded,damaged,L"x64/npc-navigation/world9.txt") && !damaged);
    assert(loaded.npcs[1].life.activity==Society::Wandering && loaded.npcs[1].life.navigation.points.empty());
    std::cout<<"Navigation tests passed: 234 routes, blocked goals, replans, bounded search, diverse leisure/tasks; resting="
             <<resting<<" strolling="<<walking<<" seconds="
             <<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<'\n';
    if(argc>1)
    {
        std::string source=argv[1];bool damaged=false;Game::World live;
        assert(Game::Load(live,damaged,std::wstring(source.begin(),source.end())) && !damaged);
        live.levelOne.active=live.storyBattle.active=false;
        Society::Initialize(live);
        std::vector<double> timings;
        int moves=0,rests=0,strolls=0;
        std::array<int,4> actualHeadings={};
        for(int tick=0;tick<600;++tick)
        {
            auto startTime=std::chrono::steady_clock::now();
            Society::Update(live,.1,false);
            timings.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-startTime).count());
            for(const auto& npc:live.npcs)if(npc.life.alive)
            {
                assert(Game::Walkable(npc.p));
                assert(npc.life.navigation.points.size()<=96);
                if(npc.motion.moving){++moves;++actualHeadings[(int)npc.motion.facing];}
                rests+=npc.life.activity==Society::Resting;
                strolls+=npc.life.activity==Society::Wandering;
            }
        }
        std::sort(timings.begin(),timings.end());
        assert(moves>1000 && rests>100 && strolls>100);
        for(int count:actualHeadings)assert(count>100);
        std::cout<<"Saved-world 60s passed: records="<<live.npcs.size()<<" moving_samples="<<moves<<" rest_samples="<<rests
                 <<" stroll_samples="<<strolls<<" tick_p95_ms="<<timings[569]<<" tick_max_ms="<<timings.back()<<'\n';
    }
}
