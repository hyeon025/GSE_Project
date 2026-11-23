#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include "../SimpleGame/WorldCombat.h"

namespace
{
    bool Near(float a,float b) { return std::abs(a-b)<.001f; }

    int KillAnimal(int level,int weapon,int species)
    {
        Game::World world;
        world.levelOne.level=level;world.levelOne.weapon=weapon;
        world.wildlifeInitialized=true;
        Game::Animal animal;animal.species=species;animal.actor.id=1;
        animal.actor.p=animal.actor.home={40,0};world.wildlife.push_back(animal);
        int hits=0;
        do
        {
            world.worldShotTimer=0;
            assert(WorldCombat::Fire(world,true));
            for(int i=0;i<30 && !world.worldShots.empty();++i)WorldCombat::Update(world,.01f,false);
            assert(world.worldShots.empty());
            assert(world.wildlife[0].hitFlash==Society::HitFlashDuration);
            ++hits;
            assert(hits<50);
        }while(world.wildlife[0].health>0);
        return hits;
    }

    void CheckGrowthAndRespawn()
    {
        const int expectedCurrent[]={6,9,13,4,3,2};
        for(int species=0;species<6;++species)
        {
            assert(KillAnimal(1,0,species)>=2);
            const int hits=KillAnimal(17,20,species);
            assert(hits==expectedCurrent[species]);
            assert(KillAnimal(30,20,species)>=2);
            std::cout<<"species="<<species<<" current_saved_build_hits="<<hits<<'\n';
        }
        LevelOne::Session low,upgraded;
        low.level=upgraded.level=17;low.weapon=10;upgraded.weapon=20;
        assert(LevelOne::Damage(upgraded)/LevelOne::Damage(low)>
               LevelOne::EnemyStrength(upgraded)/LevelOne::EnemyStrength(low));
        const float strength=LevelOne::EnemyStrength(upgraded);
        upgraded.allies=8;upgraded.phaseTwo=true;upgraded.encounter=3;upgraded.dominantTrait=3;
        assert(LevelOne::EnemyStrength(upgraded)==strength);

        Game::World world;world.wildlifeInitialized=true;world.levelOne.level=17;world.levelOne.weapon=20;
        Game::Animal animal;animal.actor.id=1;animal.actor.p=animal.actor.home={0,0};
        Wildlife::InitializeStats(animal,world.levelOne);
        const float maximum=animal.maxHealth;
        animal.health*=.5f;world.wildlife.push_back(animal);
        world.levelOne.level=30;world.levelOne.weapon=10;world.player.p={2000,0};
        Wildlife::Update(world,.01,false);
        assert(world.wildlife[0].maxHealth==maximum && Near(Wildlife::HealthRatio(world.wildlife[0]),.5f));
        world.wildlife[0].health=0;world.wildlife[0].respawn=.01;
        Wildlife::Update(world,.02,false);
        const float respawnMaximum=Wildlife::MaxHealth(world.levelOne,0);
        assert(world.wildlife[0].health==respawnMaximum && world.wildlife[0].maxHealth==respawnMaximum);
        assert(Wildlife::HealthRatio(world.wildlife[0])==1);
        Game::Animal dead;dead.health=0;
        Wildlife::InitializeStats(dead,world.levelOne);
        assert(dead.health==0); // Stat initialization is not a resurrection.
        for(int species=0;species<3;++species)
        {
            Game::World encounter;encounter.wildlifeInitialized=true;
            Game::Animal danger;danger.species=species;danger.actor.id=1;
            danger.actor.p=danger.actor.home={20,0};encounter.wildlife.push_back(danger);
            Wildlife::Update(encounter,.01,true);
            const float expected[]={8,12,18};
            assert(encounter.player.health==100-expected[species]);
        }
    }

    void CheckFieldGrowth()
    {
        for(int level:{1,17,30})for(int kind=LevelOne::Raider;kind<=LevelOne::Boss;++kind)
        {
            LevelOne::Session session;session.level=level;session.weapon=level==1?0:20;
            LevelOne::SpawnEnemy(session,kind,{60,0});
            const auto& enemy=session.enemies[0];
            assert(enemy.health==enemy.maxHealth && enemy.health>LevelOne::Damage(session));
            assert(enemy.maxHealth<=10000);
            const float maximum=enemy.maxHealth;
            session.level=30;session.weapon=20;
            assert(enemy.maxHealth==maximum); // Spawned actors retain their encounter snapshot.
        }
        LevelOne::Session special;
        LevelOne::Enemy boss;boss.kind=LevelOne::Boss;boss.maxHealth=9500;boss.health=4750;
        special.enemies.push_back(boss);
        special.level=30;special.weapon=20;
        LevelOne::RebalanceLegacyEnemies(special);
        assert(special.enemies[0].maxHealth==9500 && special.enemies[0].health==4750);
        special.enemies[0].maxHealth=10;special.enemies[0].health=0;
        LevelOne::RebalanceLegacyEnemies(special);
        assert(special.enemies[0].health==0);
    }

    void CheckSaveMigration()
    {
        Game::World old;
        LevelOne::Start(old.levelOne,123);
        old.levelOne.level=17;old.levelOne.weapon=20;
        old.levelOne.enemies.resize(1);
        old.levelOne.enemies[0].maxHealth=40;old.levelOne.enemies[0].health=10;
        LevelOne::Start(old.storyBattle,321);
        old.storyBattle.active=false;old.storyBattle.encounter=1;
        old.storyBattle.level=9;old.storyBattle.weapon=12;
        old.storyBattle.enemies.resize(1);
        old.storyBattle.enemies[0].maxHealth=31;old.storyBattle.enemies[0].health=15.5;
        assert(Game::Save(old,L"x64/balance/legacy-source.txt"));
        std::ifstream input("x64/balance/legacy-source.txt");
        std::string text((std::istreambuf_iterator<char>(input)),{});
        assert(text.find("GSE_WORLD 10\n")==0);
        text.replace(0,text.find('\n'),"GSE_WORLD 9");
        {std::ofstream output("x64/balance/legacy9.txt");output<<text;}
        Game::World migrated;bool damaged=false;
        assert(Game::Load(migrated,damaged,L"x64/balance/legacy9.txt") && !damaged);
        assert(migrated.levelOne.enemies[0].maxHealth>40);
        assert(Near(migrated.levelOne.enemies[0].health/migrated.levelOne.enemies[0].maxHealth,.25f));
        assert(migrated.storyBattle.enemies[0].maxHealth>31);
        assert(Near(migrated.storyBattle.enemies[0].health/migrated.storyBattle.enemies[0].maxHealth,.5f));
        const float health=migrated.levelOne.enemies[0].health;
        const float maximum=migrated.levelOne.enemies[0].maxHealth;
        // Upgrading during an encounter must not refill or rescale surviving saved monsters.
        migrated.levelOne.level=30;
        for(int reload=0;reload<3;++reload)
        {
            assert(Game::Save(migrated,L"x64/balance/current10.txt"));
            assert(Game::Load(migrated,damaged,L"x64/balance/current10.txt") && !damaged);
            assert(migrated.levelOne.enemies[0].health==health && migrated.levelOne.enemies[0].maxHealth==maximum);
        }
    }
}

int main(int argc,char** argv)
{
    CheckGrowthAndRespawn();CheckFieldGrowth();CheckSaveMigration();
    if(argc>1)
    {
        Game::World actual;bool damaged=false;
        const std::string source=argv[1];
        assert(Game::Load(actual,damaged,std::wstring(source.begin(),source.end())) && !damaged);
        std::cout<<"read_only_actual_save level="<<actual.levelOne.level<<" weapon="<<actual.levelOne.weapon
                 <<" damage="<<LevelOne::Damage(actual.levelOne)<<'\n';
        for(int species=0;species<6;++species)
            assert(Wildlife::MaxHealth(actual.levelOne,species)>LevelOne::Damage(actual.levelOne));
        for(const auto& enemy:actual.storyBattle.enemies)
            std::cout<<"migrated_story_enemy hp="<<enemy.health<<" max="<<enemy.maxHealth<<'\n';
    }
    std::cout<<"Monster balance: initial/current/max growth, real hit counts, upgrade value, spawn snapshots, respawn, damage tiers and save migration passed\n";
}
