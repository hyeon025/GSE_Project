#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <sstream>
#include "../SimpleGame/GameStory.h"

void CheckSuccessionAndStory()
{
    Game::World world;
    world.levelOne.active = false;
    world.player.echo[1] = 12;
    Game::Die(world, Story::VillageRaid);
    assert(world.npcs.size() == 1);
    assert(world.story.deaths.size() == 1);
    assert(world.npcs[0].originDeathId == 1);
    assert(world.story.deaths[0].successorId == world.npcs[0].id);
    assert(world.npcs[0].echo[1] == 12);
    assert(world.player.echo[1] == 0);
    assert(world.story.stage == Story::Successors);

    Story::Talk(world, world.npcs[0]);
    Story::Talk(world, world.npcs[0]);
    assert(Story::Count(world, 1) == 1);
    for (int i = 0; i < 2; ++i)
    {
        Game::Die(world);
        Story::Talk(world, world.npcs.back());
    }
    assert(world.story.stage == Story::Church);
    Story::Investigate(world, Game::GetSite(0, 0));
    Story::Investigate(world, Game::GetSite(-1, 0));
    Story::Investigate(world, Game::GetSite(0, -1));
    assert(world.story.stage == Story::Abelon);

    Story::BeginBattle(world, 123);
    assert(world.storyBattle.encounter == 2);
    assert(!world.levelOne.active);
    world.storyBattle.phase = LevelOne::Cleared;
    Story::FinishBattle(world);
    assert(world.story.stage == Story::Child);
    assert(world.npcs.back().origin == 1);
    assert(world.story.childId == 0);
    size_t before = world.npcs.size();
    Story::FinishBattle(world);
    assert(world.npcs.size() == before);

    world.storyBattle.active = false;
    Game::Die(world);
    assert(world.npcs.size() == before + 1);
    assert(world.story.childId == world.npcs.back().id);
    Story::Talk(world, world.npcs.back());
    assert(world.story.stage == Story::Ruins);
    for (const auto& key : Story::RuinKeys())
    {
        auto site = Game::GetSite(key.first, key.second);
        world.states[key].stage = 1;
        assert(Story::CanInvestigate(world, site));
        Story::Investigate(world, site);
        assert(!Story::CanInvestigate(world, site));
    }
    assert(world.story.relics == 63 && world.story.stage == Story::Truth);
    Story::Talk(world, world.npcs.back());
    assert(world.story.childComplete);
    Story::Advance(world);
    Story::Advance(world);
    Story::Advance(world);
    assert(world.story.stage == Story::FirstGrave);
    Story::BeginBattle(world, 456);
    assert(world.storyBattle.encounter == 3 && world.story.stage == Story::Enoch);

    auto& battle = world.storyBattle;
    battle.phaseTwo = true;
    battle.judgementPending = true;
    std::stringstream combat;
    combat << std::setprecision(17);
    LevelOne::Write(combat, battle, true);
    LevelOne::Session restored;
    assert(LevelOne::Read(combat, restored, true));
    assert(restored.judgementPending && !restored.judgementSeen && restored.phaseTwo);
    assert(restored.encounter == 3);

    std::stringstream legacy;
    LevelOne::Write(legacy, world.levelOne);
    assert(LevelOne::Read(legacy, restored));
    assert(restored.encounter == 0 && !restored.judgementPending);

    std::stringstream story;
    Story::Write(story, world.story);
    Story::State loaded;
    assert(Story::Read(story, loaded));
    assert(loaded.childId == world.story.childId && loaded.relics == 63);
    assert(loaded.deaths.size() == world.story.deaths.size());
}

void CheckEndings()
{
    Game::World world;
    world.levelOne.active = false;
    world.story.stage = Story::EndingChoice;
    Story::ChooseEnding(world, Story::Freedom);
    assert(world.story.ending == Story::None);
    for (int i = 0; i < 10; ++i)
    {
        world.player.echo.fill(1);
        Game::Die(world);
        world.npcs.back().saved = i < 5;
        world.npcs.back().changed = i < 3;
    }
    world.story.childComplete = true;
    world.story.childId = 1;
    world.story.relics = 63;
    assert(Story::FreedomReady(world));
    Story::ChooseEnding(world, Story::Freedom);
    assert(world.story.stage == Story::Aftermath);
    Game::Die(world);
    assert(world.npcs.back().freeWill && world.npcs.size() == 11);

    Game::World mortal;
    mortal.levelOne.active = false;
    mortal.story.stage = Story::EndingChoice;
    Story::ChooseEnding(mortal, Story::Destroy);
    mortal.player.bread = 8;
    mortal.player.fragments = 6;
    Game::Die(mortal);
    assert(mortal.npcs.empty() && mortal.story.deaths.back().successorId == 0);
    assert(mortal.player.bread == 4 && mortal.player.fragments == 3 && mortal.player.health == 50);
    mortal.time = 180;
    Story::UpdateWorld(mortal);
    assert(mortal.npcs.size() == 1 && mortal.npcs.back().origin == 2);
    assert(mortal.npcs.back().originDeathId == 0);

    Story::State invalid;
    invalid.stage = Story::Aftermath;
    std::stringstream corrupt;
    Story::Write(corrupt, invalid);
    Story::State rejected;
    assert(!Story::Read(corrupt, rejected));
}

int main()
{
    CheckSuccessionAndStory();
    CheckEndings();
}
