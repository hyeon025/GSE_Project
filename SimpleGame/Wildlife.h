#pragma once
#include "WorldSimulation.h"

namespace Wildlife
{
    inline float MaxHealth(const LevelOne::Session& growth, int species)
    {
        static const float base[] = {60,84,132,36,24,18};
        return base[std::max(0,std::min(5,species))] * LevelOne::EnemyStrength(growth);
    }
    inline void InitializeStats(Game::Animal& animal, const LevelOne::Session& growth)
    {
        if(animal.balanceInitialized)return;
        const float ratio=animal.maxHealth>0 ? std::max(0.f,std::min(1.f,animal.health/animal.maxHealth)) : 0.f;
        animal.maxHealth=MaxHealth(growth,animal.species);
        animal.health=animal.maxHealth*ratio;
        animal.balanceInitialized=true;
    }
    inline float HealthRatio(const Game::Animal& animal)
    {
        return animal.maxHealth>0 ? std::max(0.f,std::min(1.f,animal.health/animal.maxHealth)) : 0.f;
    }
    inline float PlayerDamage(int species)
    {
        static const float damage[]={8,12,18};
        return damage[std::max(0,std::min(2,species))];
    }
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
            InitializeStats(animal,w.levelOne);
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
            InitializeStats(a,w.levelOne);
            a.cooldown=std::max(0.0,a.cooldown-dt);
            a.hitFlash=std::max(0.f,a.hitFlash-(float)dt);
            if(a.health<=0)
            {
                a.respawn=std::max(0.0,a.respawn-dt);
                if(a.respawn<=0 && Game::Distance(w.player.p,a.actor.home)>220)
                {
                    a.health=a.maxHealth;a.balanceInitialized=false;
                    InitializeStats(a,w.levelOne); // A new encounter snapshots current growth.
                    a.hitFlash=0;a.cooldown=0;
                    a.actor.p=a.actor.home;a.chasing=false;a.actor.life.target=0;
                }
                continue;
            }
            double distance=Game::Distance(a.actor.p,w.player.p);
            auto before=a.actor.p;
            bool danger=a.species<3;
            const int previousTarget=a.actor.life.target;
            a.actor.life.target=0;
            if(danger && Game::Distance(a.actor.p,a.actor.home)<380)
            {
                auto* resident=Society::Find(w,previousTarget);
                if(resident && resident->life.alive &&
                   (resident->id!=w.story.childId || w.story.childComplete) &&
                   Game::Distance(a.actor.p,resident->p)<300)
                {
                    a.actor.life.target=resident->id;
                }
                else if(playerPresent && distance<(a.chasing && previousTarget<=0 ? 300 : 155))
                {
                    a.actor.life.target=-1;
                }
                else
                {
                    double nearestSquared=155.0*155.0;
                    for(const auto& npc:w.npcs)
                    {
                        if(!npc.life.alive || (npc.id==w.story.childId && !w.story.childComplete))continue;
                        const double dx=npc.p.x-a.actor.p.x, dy=npc.p.y-a.actor.p.y;
                        const double distanceSquared=dx*dx+dy*dy;
                        if(distanceSquared>=nearestSquared)continue;
                        nearestSquared=distanceSquared;
                        a.actor.life.target=npc.id;
                    }
                }
            }
            a.chasing=a.actor.life.target!=0;
            if(!a.chasing && distance>1300)
            {
                a.moving=false;
                a.actor.motion.Update(0,0,0,0);
                continue;
            }
            if(a.chasing)
            {
                auto* resident=Society::Find(w,a.actor.life.target);
                a.destination=resident ? resident->p : w.player.p;
                if(Game::Distance(a.actor.p,a.destination)>27 || !Society::ClearSegment(a.actor.p,a.destination))
                    Society::Move(a.actor,a.destination,dt*1.9,&w.society.navigationBudget);
                else if(resident && a.cooldown<=0)
                {
                    resident->life.health=std::max(0.f,resident->life.health-5);
                    resident->life.hitFlash=Society::HitFlashDuration;
                    a.cooldown=1.8;
                    w.dirty=true;
                    if(resident->life.health<=0)
                        Society::Die(w,*resident,Society::WildlifeKiller);
                }
                else if(!resident && a.cooldown<=0 && w.wildlifePlayerCooldown<=0)
                {
                    w.player.health=std::max(0.f,w.player.health-PlayerDamage(a.species));
                    a.cooldown=1.8;w.wildlifePlayerCooldown=1;
                    if(w.player.health<=0)Game::Die(w,Story::VillageRaid);
                }
            }
            else
            {
                if(previousTarget!=0)a.wander=0;
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
            if(a.chasing)a.yaw=Yaw({a.destination.x-a.actor.p.x,a.destination.y-a.actor.p.y});
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
            InitializeStats(a,w.levelOne);
            double d=Game::Distance(a.actor.p,w.player.p);
            if(a.health>0 && d<best && NpcNavigation::Clear(w.player.p,a.actor.p)) {best=d;nearest=&a;}
        }
        if(!nearest)return false;
        nearest->health=std::max(0.f,nearest->health-LevelOne::Damage(w.levelOne));nearest->chasing=nearest->species<3;
        nearest->hitFlash=Society::HitFlashDuration;
        w.wildlifePlayerCooldown=.5;
        if(nearest->health<=0){nearest->respawn=45;w.player.bread=std::min(1000,w.player.bread+1);++w.player.echo[0];}
        w.dirty=true;return true;
    }
}
