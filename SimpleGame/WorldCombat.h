#pragma once
#include "Wildlife.h"

namespace WorldCombat
{
    constexpr float AnimationDuration = .4f;
    inline float Range(const Game::World& w) { return LevelOne::Range(w.levelOne); }
    inline bool Fire(Game::World& w,bool manual=false)
    {
        if(w.worldShotTimer>0)return false;
        Game::Animal* target=nullptr;double best=Range(w);
        for(auto& a:w.wildlife)
        {
            double d=Game::Distance(w.player.p,a.actor.p);
            if(a.health>0 && (manual || a.species<3 || a.chasing) && d<=best && NpcNavigation::Clear(w.player.p,a.actor.p))
            {target=&a;best=d;}
        }
        if(!target)return false;
        Game::Vec delta(target->actor.p.x-w.player.p.x,target->actor.p.y-w.player.p.y);
        double length=std::max(.001,std::hypot(delta.x,delta.y));
        LevelOne::Projectile shot;
        shot.p={w.player.p.x,w.player.p.y};shot.direction={delta.x/length,delta.y/length};
        shot.remaining=Range(w);shot.damage=LevelOne::Damage(w.levelOne);shot.speed=430;shot.hostile=false;
        w.worldShots.push_back(shot);w.worldShotTimer=LevelOne::Cooldown(w.levelOne);
        w.worldAttackAnimation=AnimationDuration;w.worldAttackAim=delta;
        return true;
    }
    inline void Update(Game::World& w,float dt,bool automatic=true)
    {
        dt=std::max(0.f,std::min(.1f,dt));
        w.worldShotTimer=std::max(0.f,w.worldShotTimer-dt);
        w.worldAttackAnimation=std::max(0.f,w.worldAttackAnimation-dt);
        if(automatic)Fire(w);
        for(auto& shot:w.worldShots)
        {
            double travel=std::min((double)shot.remaining,(double)shot.speed*dt);
            int steps=std::max(1,(int)std::ceil(travel/4));
            for(int i=0;i<steps && shot.remaining>0;++i)
            {
                Game::Vec before(shot.p.x,shot.p.y);
                shot.p.x+=shot.direction.x*travel/steps;shot.p.y+=shot.direction.y*travel/steps;
                shot.remaining-=(float)(travel/steps);
                if(!NpcNavigation::Clear(before,{shot.p.x,shot.p.y})) {shot.remaining=0;break;}
                for(auto& animal:w.wildlife)
                {
                    if(animal.health<=0 || Game::Distance(animal.actor.p,{shot.p.x,shot.p.y})>(animal.species==2?23:16))continue;
                    animal.health=std::max(0.f,animal.health-shot.damage);
                    animal.hitFlash=.18f;
                    animal.chasing=animal.species<3;shot.remaining=0;w.dirty=true;
                    if(animal.health<=0)
                    {animal.respawn=45;w.player.bread=std::min(1000,w.player.bread+1);++w.player.echo[0];}
                    break;
                }
            }
        }
        w.worldShots.erase(std::remove_if(w.worldShots.begin(),w.worldShots.end(),[](const LevelOne::Projectile& p){return p.remaining<=0;}),w.worldShots.end());
    }
}
