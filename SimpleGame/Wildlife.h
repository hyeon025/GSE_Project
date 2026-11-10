#pragma once
#include "WorldSimulation.h"

namespace Wildlife
{
    inline const char* Asset(int species)
    {
        static const char* names[]={"MOB_Wolf","MOB_Boar","MOB_Bear","MOB_Stag","MOB_Goat","MOB_Hare"};
        return names[species%6];
    }
    // Exported head/muzzle points toward glTF +Z, converted to game -Y.
    inline float Yaw(Game::Vec direction)
    {
        return (float)(std::atan2(direction.y,direction.x)+1.5707963267948966);
    }
    inline void Initialize(Game::World& w)
    {
        if(w.wildlifeInitialized)return;
        for(int v=0;v<Society::VillageCount;++v)for(int species=0;species<6;++species)
        {
            Game::Animal animal;animal.species=species;animal.actor.id=v*6+species+1;
            auto center=Game::VillageCenter(v);
            for(int attempt=0;attempt<64;++attempt)
            {
                unsigned h=Game::Hash(v,species,831+attempt);
                double angle=(h%65536)*6.283185307179586/65536;
                double radius=species<3 ? 680+(h/31)%130 : 250+(h/31)%380;
                Game::Vec p(center.x+std::cos(angle)*radius,center.y+std::sin(angle)*radius);
                if(Game::Walkable(p)) {animal.actor.p=animal.actor.home=p;break;}
            }
            if(Game::Distance(animal.actor.home,center)<100)continue;
            animal.destination=animal.actor.home;
            animal.actor.life.activity=Society::Wandering;
            animal.wander=animal.actor.id%7;
            w.wildlife.push_back(animal);
        }
        w.wildlifeInitialized=true;
    }
    inline void Update(Game::World& w,double dt,bool playerPresent)
    {
        Initialize(w);
        w.wildlifePlayerCooldown=std::max(0.0,w.wildlifePlayerCooldown-dt);
        for(auto& a:w.wildlife)
        {
            a.cooldown=std::max(0.0,a.cooldown-dt);
            a.hitFlash=std::max(0.f,a.hitFlash-(float)dt);
            if(a.health<=0)
            {
                a.respawn=std::max(0.0,a.respawn-dt);
                if(a.respawn<=0 && Game::Distance(w.player.p,a.actor.home)>220)
                {a.health=60;a.actor.p=a.actor.home;a.chasing=false;}
                continue;
            }
            double distance=Game::Distance(a.actor.p,w.player.p);
            if(distance>1300){a.actor.motion.Update(0,0,0,0);continue;}
            auto before=a.actor.p;
            bool danger=a.species<3;
            a.chasing=playerPresent && danger && distance< (a.chasing?300:155) && Game::Distance(a.actor.p,a.actor.home)<380;
            if(a.chasing)
            {
                a.destination=w.player.p;a.actor.life.target=-1;
                if(distance>27)Society::Move(a.actor,a.destination,dt*1.9,&w.society.navigationBudget);
                else if(a.cooldown<=0 && w.wildlifePlayerCooldown<=0)
                {
                    w.player.health=std::max(0.f,w.player.health-5);
                    a.cooldown=1.8;w.wildlifePlayerCooldown=1;
                    if(w.player.health<=0)Game::Die(w,Story::VillageRaid);
                }
            }
            else
            {
                a.actor.life.target=0;
                a.wander-=dt;
                if(a.wander<=0)
                {
                    a.destination=Society::NearbyPoint(a.actor,a.actor.home,120,(unsigned)(w.time/3)+a.actor.id);
                    a.wander=5+a.actor.id%6;
                }
                if(!danger && playerPresent && distance<95)
                {
                    Game::Vec away(a.actor.p.x-w.player.p.x,a.actor.p.y-w.player.p.y);
                    double length=std::max(1.0,std::hypot(away.x,away.y));
                    Game::Vec flee(a.actor.p.x+away.x/length*80,a.actor.p.y+away.y/length*80);
                    if(Game::Walkable(flee))a.destination=flee;
                }
                Society::Move(a.actor,a.destination,dt,&w.society.navigationBudget);
            }
            Game::Vec travel(a.actor.p.x-before.x,a.actor.p.y-before.y);
            a.moving=std::hypot(travel.x,travel.y)>.001;
            if(a.chasing)a.yaw=Yaw({w.player.p.x-a.actor.p.x,w.player.p.y-a.actor.p.y});
            else if(a.moving)a.yaw=Yaw(travel);
            a.actor.motion.Update(travel.x,travel.y,travel.x,travel.y,a.chasing);
        }
    }
    inline bool Attack(Game::World& w)
    {
        if(w.wildlifePlayerCooldown>0)return false;
        Game::Animal* nearest=nullptr;double best=65;
        for(auto& a:w.wildlife)
        {
            double d=Game::Distance(a.actor.p,w.player.p);
            if(a.health>0 && d<best && NpcNavigation::Clear(w.player.p,a.actor.p)) {best=d;nearest=&a;}
        }
        if(!nearest)return false;
        nearest->health=std::max(0.f,nearest->health-25);nearest->chasing=nearest->species<3;
        w.wildlifePlayerCooldown=.5;
        if(nearest->health<=0){nearest->respawn=45;w.player.bread=std::min(1000,w.player.bread+1);++w.player.echo[0];}
        w.dirty=true;return true;
    }
}
