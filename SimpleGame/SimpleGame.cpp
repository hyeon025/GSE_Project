/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)
Distributed under the What The Hell License.
*/
#include "stdafx.h"
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include "Dependencies/GLEW/glew-2.2.0/include/GL/glew.h"
#define GLFW_INCLUDE_NONE
#include "Dependencies/GLFW/glfw-3.4/include/GLFW/glfw3.h"
#include "Renderer.h"
#include "GameWorld.h"
#include "GameStory.h"
#include "GameScene.h"
#include "LevelOneView.h"
#include "WorldMonitor.h"

using namespace Game;
World g_World;
std::unique_ptr<Renderer> g_Renderer;
std::unique_ptr<GameScene> g_Scene;
std::unique_ptr<LevelOneView> g_LevelView;
std::unique_ptr<WorldMonitor> g_Monitor;
std::vector<Site> g_Sites;
bool g_Keys[256] = {};
double g_LastTick = 0;
int g_Width = 1440, g_Height = 900;
GLFWwindow* g_Window = nullptr;
double g_UpdateMs = 0, g_PreviousSwapMs = 0;
int g_TargetKind = -1, g_TargetIndex = -1, g_Panel = 0;
int g_JournalOffset = 0;
int g_AffinityPage = 0, g_AffinityVillage = -1, g_AffinitySelected = 0;
bool g_AffinityMetOnly = false, g_AffinityDescending = true;
double g_AffinityRefresh = -1;
uint64_t g_AffinityRevision = 0;
size_t g_AffinityPopulation = 0;
std::vector<size_t> g_AffinityRanking;
PlayerProfile g_ProfileDraft;
int g_ProfileField = 0;
bool g_ProfileSelected = false;
CharacterFacing g_ProfileFacing = CharacterFacing::Front;
Ability g_Secondary = Matter;
int g_MouseX = 0, g_MouseY = 0;
bool g_Dialog = false, g_DeathConfirm = false, g_SaveBlocked = false, g_Focused = true;
bool g_NpcEconomy = false;
double g_Autosave = 0;
float g_UiScale = 1;
std::wstring g_Toast;
float g_ToastTime = 0;
Color g_Ink(.90f, .91f, .86f), g_Muted(.65f, .69f, .66f), g_Accent(.79f, .72f, .46f);

struct Button
{
    float x, y, w, h;
    int action;
    bool enabled;
    std::wstring label;
};

std::vector<Button> g_Buttons;

struct Choice
{
    std::wstring text;
    int action;
    bool enabled;
};

enum Action
{
    Open = 1,
    Close,
    Journal,
    Abilities,
    SaveNow,
    DeathQuestion,
    DeathAccept,
    CastInnate,
    CastAdjacent,
    SelectMind,
    SelectMatter,
    Repair = 20,
    Rest,
    PickHerb,
    TakeCache,
    HealHerb,
    CarryWounded,
    Rob,
    ReadRelic,
    ReadFate,
    Offer,
    Train,
    Talk = 40,
    Share,
    TrainNpc,
    EnterLevel = 60,
    StoryOpen = 70,
    StoryAdvance,
    StoryBattle,
    StoryInvestigate,
    RescueNpc,
    ChangeNpc,
    ThreatenNpc,
    EndDestroy,
    EndSuccessor,
    EndFreedom,
    NpcEconomy,
    NpcSocial,
    BuyFood,
    SellHerb,
    AttackNpc,
    AvatarOpen = 100,
    AvatarApply,
    AvatarCancel,
    AvatarName,
    AvatarMotto,
    AvatarTurn,
    AffinityOpen = 120,
    AffinityPrevious,
    AffinityNext,
    AffinitySort,
    AffinityVillage,
    AffinityMet,
    AvatarHairColor = 200,
    AvatarSkin = 220,
    AvatarHairShape = 240,
    AvatarExpression = 260,
    AvatarOutfit = 280,
    AvatarCloth = 300,
    AffinitySelect = 1000
};

void AbilityUse(bool secondary);

void InputProfileCharacter(unsigned code)
{
    if (g_Panel != 4 || !g_ProfileField || code < 32 || code == 127 || code > 0x10ffff ||
        (code >= 0xd800 && code <= 0xdfff))
    {
        return;
    }
    auto& text = g_ProfileField == 1 ? g_ProfileDraft.name : g_ProfileDraft.motto;
    if (g_ProfileSelected)
    {
        text.clear();
        g_ProfileSelected = false;
    }
    AppendProfileCharacter(text, code, g_ProfileField == 1 ? PlayerProfile::NameLimit : PlayerProfile::MottoLimit);
}

bool InCombat()
{
    return g_World.levelOne.active || g_World.storyBattle.active;
}

LevelOne::Session& CurrentLevel()
{
    return g_World.storyBattle.active ? g_World.storyBattle : g_World.levelOne;
}

float UiWidth()
{
    return g_Width / g_UiScale;
}

float UiHeight()
{
    return g_Height / g_UiScale;
}

void Box(float x, float y, float w, float h, Color c)
{
    g_Renderer->Rect(x * g_UiScale, y * g_UiScale, w * g_UiScale, h * g_UiScale, c);
}

void Line(float ax, float ay, float bx, float by, float width, Color c)
{
    g_Renderer->Line(ax * g_UiScale, ay * g_UiScale, bx * g_UiScale, by * g_UiScale, width * g_UiScale, c);
}

void Text(float x, float y, const std::wstring& value, Color c, int size = 16)
{
    g_Renderer->Text(x * g_UiScale, y * g_UiScale, value, c, std::max(8, (int)(size * g_UiScale)));
}

void Notify(const std::wstring& s)
{
    g_Toast = s;
    g_ToastTime = 5;
    if (InCombat())
    {
        LevelOne::Notice(CurrentLevel(), s);
    }
}

bool Persist()
{
    if (g_SaveBlocked)
    {
        Notify(
            L"\uae30\uc874 \uc800\uc7a5\uc744 \uc77d\uc744 \uc218 \uc5c6\uc5b4 \ub36e\uc5b4\uc4f0\uae30\ub97c \uc911\ub2e8\ud588\uc2b5\ub2c8\ub2e4."
        );
        return false;
    }
    if (!Save(g_World))
    {
        Notify(
            L"\uc800\uc7a5\ud558\uc9c0 \ubabb\ud588\uc2b5\ub2c8\ub2e4. \uc800\uc7a5 \ud3f4\ub354\ub97c \ud655\uc778\ud574\uc8fc\uc138\uc694."
        );
        return false;
    }
    return true;
}

void RefreshTargets()
{
    if (g_Dialog)
    {
        return;
    }
    g_Sites = Sites(g_World.player.p, 3);
    double best = 64;
    g_TargetKind = -1;
    g_TargetIndex = -1;
    for (size_t i = 0; i < g_Sites.size(); ++i)
    {
        double d = Distance(g_World.player.p, g_Sites[i].p);
        if (d < best)
        {
            best = d;
            g_TargetKind = 1;
            g_TargetIndex = (int)i;
        }
    }
    for (size_t i = 0; i < g_World.npcs.size(); ++i)
    {
        if (!g_World.npcs[i].life.alive)
        {
            continue;
        }
        double d = Distance(g_World.player.p, g_World.npcs[i].p);
        if (d < best)
        {
            best = d;
            g_TargetKind = 2;
            g_TargetIndex = (int)i;
        }
    }
}

unsigned NewLevelSeed()
{
    std::random_device random;
    unsigned seed = random();
    return seed ? seed : 1;
}

void LevelCommand(LevelOneView::Command command)
{
    if (!g_LevelView)
    {
        return;
    }
    auto& level = CurrentLevel();
    switch (command)
    {
        case LevelOneView::TogglePause:
            if (level.phase == LevelOne::Fighting)
            {
                g_LevelView->paused = !g_LevelView->paused;
            }
            break;
        case LevelOneView::Resume:
            if (level.judgementPending)
            {
                level.judgementPending = false;
                level.judgementSeen = true;
                Persist();
            }
            g_LevelView->paused = false;
            break;
        case LevelOneView::Retry:
            if (level.phase == LevelOne::Fighting)
            {
                return;
            }
            if (level.encounter)
            {
                Story::BeginBattle(g_World, NewLevelSeed());
            }
            else
            {
                LevelOne::Start(level, NewLevelSeed());
            }
            g_LevelView->paused = false;
            Persist();
            break;
        case LevelOneView::SaveGame:
            if (Persist())
            {
                LevelOne::Notice(
                    level,
                    L"\uacbd\uc791\uc9c0\uc758 \uc9c4\ud589 \uc0c1\ud669\uc744 \uc800\uc7a5\ud588\uc2b5\ub2c8\ub2e4."
                );
            }
            break;
        case LevelOneView::Leave:
            level.active = false;
            g_LevelView->paused = false;
            g_Panel = level.encounter && level.phase != LevelOne::Fighting ? 3 : 0;
            g_Scene->cameraOverride = false;
            RefreshTargets();
            Persist();
            break;
        default:
            return;
    }
    std::fill(std::begin(g_Keys), std::end(g_Keys), false);
    g_Buttons.clear();
}

std::wstring SiteName(const Site& s)
{
    auto st = State(g_World, s.key);
    switch (s.kind)
    {
        case Camp:
            return st.stage ? L"\ubd88\ube5b\uc774 \ub3cc\uc544\uc628 \uc57c\uc601\uc9c0"
                            : L"\uaebc\uc9c4 \uc57c\uc601\uc9c0";
        case Wounded:
            return st.stage == 1   ? L"\uc0b4\uc544\ub0a8\uc740 \uc21c\ub840\uc790"
                   : st.stage == 2 ? L"\uc785\uc744 \ub2eb\uc740 \ud589\uc778"
                                   : L"\ubd80\uc0c1\ub2f9\ud55c \ud589\uc778";
        case Herb:
            return L"\uc740\uc78e \uc57d\ucd08 \uad70\ub77d";
        case Cache:
            return st.stage ? L"\ube44\uc6cc\uc9c4 \uc218\ub808" : L"\ubc84\ub824\uc9c4 \ubcf4\uae09 \uc218\ub808";
        case Relic:
            return L"\uae30\uc5b5\uc774 \ub0a8\uc740 \ud3d0\ud5c8";
        default:
            return L"\uc774\ub984 \uc5c6\ub294 \uc790\ub4e4\uc758 \uc81c\ub2e8";
    }
}

std::wstring TargetName()
{
    if (g_TargetKind == 1)
    {
        return SiteName(g_Sites[g_TargetIndex]);
    }
    if (g_TargetKind == 2)
    {
        if (g_World.npcs[g_TargetIndex].id == g_World.story.childId)
        {
            return g_World.npcs[g_TargetIndex].Name() +
                   L" / \uae30\uc5b5\uc758 \ud30c\ud3b8\uc744 \uc9c0\ub2cc \uc544\uc774";
        }
        const auto& n = g_World.npcs[g_TargetIndex];
        return n.Name() + L" / " + Society::JobName(n.life.job) + (n.origin == 0 ? L"  \uacc4\uc2b9\uc790" : L"");
    }
    return L"";
}

std::vector<Choice> Choices()
{
    std::vector<Choice> c;
    const auto& p = g_World.player;
    if (g_DeathConfirm)
    {
        return {
            {L"\uc774 \uc0b6\uc744 \ub9c8\uce5c\ub2e4", DeathAccept, true},
            {L"\uc0b4\uc544\uac08 \uae38\uc744 \ub354 \ucc3e\ub294\ub2e4", Close, true}
        };
    }
    if (g_TargetKind == 2)
    {
        const auto& n = g_World.npcs[g_TargetIndex];
        if (!n.life.alive || Distance(n.p, p.p) > 90)
        {
            return c;
        }
        if (g_NpcEconomy)
        {
            bool trade = Society::CanTrade(g_World, n);
            c.push_back(
                {L"\ube75 \uad6c\ub9e4  /  \uc740\ud654 4",
                 BuyFood,
                 trade && n.life.goods > 0 && g_World.society.gold >= 4}
            );
            c.push_back(
                {L"\uc57d\ucd08 \ud310\ub9e4  /  \uc740\ud654 3", SellHerb, trade && p.herbs > 0 && n.life.gold >= 3}
            );
            c.push_back(
                {L"\uacf5\uaca9",
                 AttackNpc,
                 g_World.society.playerCooldown <= 0 && Distance(n.p, p.p) <= 65 &&
                     (n.id != g_World.story.childId || g_World.story.childComplete)}
            );
            c.push_back({L"\ub300\ud654", NpcSocial, true});
            return c;
        }
        c.push_back({L"\uac70\ub798 / \uc804\ud22c", NpcEconomy, true});
        double talkWait = Affinity::Remaining(g_World.society.elapsed, n.affinity.lastTalk, Affinity::TalkCooldown);
        std::wstring talkReward = n.relationship >= 100 ? L"\ucd5c\ub300 \ud638\uac10\ub3c4" : talkWait > 1e-8
            ? L"\ubcf4\uc0c1\uae4c\uc9c0 " + std::to_wstring((int)std::ceil(talkWait)) + L"\ucd08" : L"\ud638\uac10\ub3c4 +5";
        c.push_back({L"\uc0b4\uc544\uc628 \uc774\uc57c\uae30\ub97c \ub4e3\ub294\ub2e4  \u00b7  " + talkReward, Talk, true});
        c.push_back({L"\ube75\uc744 \ub098\ub208\ub2e4  \u00b7  \ube75 1", Share, p.bread > 0 && n.relationship < 100});
        bool related = Adjacent(p.ability, n.ability);
        c.push_back(
            {related ? std::wstring(AbilityName(n.ability)) +
                           L" \uacc4\uc5f4 \uc218\ub828  \u00b7  \uc2e0\ub8b0 1, \uae30\ub825 25"
                     : L"\uc774 \uacc4\uc5f4\uc740 \ub0b4 \uace0\uc720\ub2a5\ub825\uacfc \uba40\ub2e4",
             TrainNpc,
             related && n.trust > 0 && !p.training[n.ability] && p.stamina >= 25}
        );
        c.push_back(
            {L"\uc0c1\ucc98\ub97c \ub3cc\ubcf4\uace0 \ubcf4\ud638\ud55c\ub2e4 \u00b7 \uc57d\ucd08 1",
             RescueNpc,
             n.met && !n.saved && p.herbs > 0}
        );
        c.push_back(
            {L"\ub2e4\ub978 \uc0b6\uc744 \uc2dc\uc791\ud558\ub3c4\ub85d \ub3d5\ub294\ub2e4 \u00b7 \ubaa9\uc7ac 2",
             ChangeNpc,
             !n.changed && n.relationship >= 50 && p.wood >= 2}
        );
        c.push_back({L"\ubcf5\uc885\uc744 \uac15\uc694\ud55c\ub2e4", ThreatenNpc, n.relationship > -100});
        return c;
    }
    if (g_TargetKind != 1)
    {
        return c;
    }
    const auto& s = g_Sites[g_TargetIndex];
    auto st = State(g_World, s.key);
    if (Story::CanInvestigate(g_World, s))
    {
        c.push_back(
            {L"\uacc4\uc2b9\uc758 \ubc95\uce59\uc5d0 \uad00\ud55c \ud754\uc801\uc744 \uc870\uc0ac\ud55c\ub2e4",
             StoryInvestigate,
             true}
        );
    }
    switch (s.kind)
    {
        case Camp:
            if (!st.stage)
            {
                c.push_back(
                    {L"\uc57c\uc601\uc9c0\ub97c \ubcf5\uad6c\ud55c\ub2e4  \u00b7  \ubaa9\uc7ac 2", Repair, p.wood >= 2}
                );
            }
            else
            {
                c.push_back({L"\uc2dd\uc0ac\ud558\uace0 \uc270\ub2e4  \u00b7  \ube75 1", Rest, p.bread > 0});
            }
            break;
        case Wounded:
            if (st.stage == 0)
            {
                c.push_back(
                    {L"\uc57d\ucd08\ub85c \uce58\ub8cc\ud55c\ub2e4  \u00b7  \uc57d\ucd08 1", HealHerb, p.herbs > 0}
                );
                c.push_back(
                    {L"\uc0c1\ucc98\ub97c \ubb34\ub985\uc4f0\uace0 \ubd80\ucd95\ud574 \uad6c\uc870\ud55c\ub2e4  \u00b7  \uccb4\ub825 18",
                     CarryWounded,
                     p.health > 18}
                );
                c.push_back({L"\uc2dd\ub7c9\uc744 \ube7c\uc557\ub294\ub2e4  \u00b7  \ube75 2 \ud68d\ub4dd", Rob, true});
            }
            else if (st.stage == 1)
            {
                c.push_back(
                    {std::wstring(AbilityName(s.ability)) +
                         L" \uacc4\uc5f4 \uc218\ub828  \u00b7  \ube75 1, \uae30\ub825 25",
                     Train,
                     Adjacent(p.ability, s.ability) && !p.training[s.ability] && p.bread > 0 && p.stamina >= 25}
                );
            }
            break;
        case Herb:
            c.push_back(
                {g_World.time >= st.readyAt
                     ? L"\ubfcc\ub9ac\ub97c \ub0a8\uae30\uace0 \ucc44\uc9d1\ud55c\ub2e4  \u00b7  \uc57d\ucd08 1"
                     : L"\uc0c8 \uc78e\uc774 \uc790\ub77c\uae30\ub97c \uae30\ub2e4\ub9b0\ub2e4",
                 PickHerb,
                 g_World.time >= st.readyAt}
            );
            break;
        case Cache:
            c.push_back(
                {L"\ub0a8\uc740 \ubcf4\uae09\ud488\uc744 \ucc59\uae34\ub2e4  \u00b7  \ubaa9\uc7ac 2, \ube75 1",
                 TakeCache,
                 st.stage == 0}
            );
            break;
        case Relic:
            c.push_back(
                {L"\uae30\uc5b5\uc5d0 \uc190\uc744 \ub304\ub2e4  \u00b7  \uccb4\ub825 16, \ud30c\ud3b8 1 \ud68d\ub4dd",
                 ReadRelic,
                 st.stage == 0}
            );
            c.push_back(
                {L"\uace0\uc720\ub2a5\ub825\uc73c\ub85c \uc9d5\uc870\ub97c \uc77d\ub294\ub2e4  \u00b7  \uae30\ub825 35",
                 ReadFate,
                 st.stage == 0 && p.ability == Fate && p.stamina >= 35}
            );
            break;
        case Shrine:
            c.push_back(
                {L"\uae30\uc5b5\uc758 \ud30c\ud3b8\uc744 \ubc14\uce5c\ub2e4  \u00b7  \ud30c\ud3b8 1",
                 Offer,
                 p.fragments > 0 && st.stage == 0}
            );
            c.push_back({L"\ub0b4 \uc0b6\uc758 \ub05d\uc744 \uc0dd\uac01\ud55c\ub2e4", DeathQuestion, true});
            break;
    }
    return c;
}

std::wstring Description()
{
    if (g_DeathConfirm)
    {
        return g_World.story.ending == Story::Destroy
                   ? L"\uacc4\uc2b9\uc758 \ubc95\uce59\uc740 \ub05d\ub0ac\ub2e4. \uc774 \uc8fd\uc74c\uc740 \uc0c8 \uacc4\uc2b9\uc790\ub97c \ub9cc\ub4e4\uc9c0 \uc54a\ub294\ub2e4. \uc2dd\ub7c9\uacfc \ud30c\ud3b8\uc758 \uc808\ubc18\uc744 \uc783\uace0, \uc0c1\ucc98\ub97c \uc548\uc740 \ucc44 \ub3cc\uc544\uc628\ub2e4."
                   : L"\ud558\ub098\uc758 \uc8fd\uc74c\uc740 \ud558\ub098\uc758 \uc0dd\uba85\uc744 \ub9cc\ub4e0\ub2e4. \uc0c8 \uc0dd\uba85\uc740 \ub098\uc758 \uae30\uc5b5\uc774 \uc544\ub2c8\ub77c \uc0b6\uc758 \ubc29\ud5a5\uc744 \ubb3c\ub824\ubc1b\ub294\ub2e4. \uc138\uacc4\uc640 \uad00\uacc4\ub294 \uacc4\uc18d\ub41c\ub2e4.";
    }
    if (g_TargetKind == 2)
    {
        auto& n = g_World.npcs[g_TargetIndex];
        return Society::Status(n) + L"\n" + AbilityName(n.ability) + L" / " + Society::VillageName(n.life.village);
    }
    const auto& s = g_Sites[g_TargetIndex];
    auto st = State(g_World, s.key);
    switch (s.kind)
    {
        case Camp:
            return st.stage
                       ? L"\ub2e4\uc2dc \ud53c\uc6b4 \ubd88\uc774 \uc5ec\ud589\uc790\ub4e4\uc744 \uae30\ub2e4\ub9b0\ub2e4. \uc5ec\uae30\uc11c \uc26c\uba74 \uc0c1\ucc98\uc640 \uae30\ub825\uc774 \ud68c\ubcf5\ub418\uace0, \ub2e4\uc74c \uc0b6\uc774 \uc2dc\uc791\ub420 \uc7a5\uc18c\ub85c \ub0a8\ub294\ub2e4."
                       : L"\uc816\uc740 \uc7ac \uc544\ub798 \uc791\uc740 \ubd88\uc528\uac00 \ub0a8\uc544 \uc788\ub2e4. \uc218\ub808\uc5d0\uc11c \ubaa9\uc7ac\ub97c \uac00\uc838\uc624\uba74 \uc774\uacf3\uc5d0 \ub2e4\uc2dc \ubd88\uc744 \ud53c\uc6b8 \uc218 \uc788\ub2e4.";
        case Wounded:
            return st.stage == 0
                       ? L"\ud589\uc778\uc774 \uc0c1\ucc98\ub97c \ub204\ub978 \ucc44 \ub5a8\uace0 \uc788\ub2e4. \uc57d\ucd08\uac00 \uc5c6\ub2e4\uba74 \uc548\uc804\ud55c \uacf3\uae4c\uc9c0 \ubd80\ucd95\ud574 \uad6c\uc870\ud560 \uc218 \uc788\ub2e4. \ubb34\ub9ac\ud55c \uad6c\uc870\ub294 \ub0b4 \ubab8\uc5d0 \uc0c1\ucc98\ub97c \ub0a8\uae34\ub2e4."
                   : st.stage == 1
                       ? std::wstring(
                             L"\ub2f9\uc2e0\uc774 \uc0b4\ub9b0 \uc21c\ub840\uc790\uac00 \ub3cc\uc544\uc654\ub2e4. \uc790\uc2e0\uc774 \ud0c0\uace0\ub09c "
                         ) + AbilityName(s.ability) +
                             L" \uacc4\uc5f4\uc758 \uc791\uc740 \uae30\uc220\uc744 \ub098\ub204\uace0 \uc2f6\uc5b4 \ud55c\ub2e4."
                       : L"\ud589\uc778\uc740 \ub2f9\uc2e0\uc744 \uc54c\uc544\ubcf4\uace0 \uc18c\uc9c0\ud488\uc744 \uac10\ucd98\ub2e4. \uae38\uc740 \uadf8\ub300\ub85c\uc9c0\ub9cc, \uad00\uacc4\ub294 \uc774\uc804\uc73c\ub85c \ub3cc\uc544\uac00\uc9c0 \uc54a\ub294\ub2e4.";
        case Herb:
            return g_World.time >= st.readyAt
                       ? L"\ucc28\uac00\uc6b4 \ub545\uc5d0\uc11c\ub3c4 \uc740\ube5b \uc78e\uc774 \ub3cb\uc558\ub2e4. \ubfcc\ub9ac\ub97c \ub0a8\uae30\uba74 \uc2dc\uac04\uc774 \ud750\ub978 \ub4a4 \ub2e4\uc2dc \ucc44\uc9d1\ud560 \uc218 \uc788\ub2e4."
                       : L"\uc78e\uc744 \uac70\ub454 \uc790\ub9ac\uc5d0 \uc5b4\ub9b0 \uc2f9\uc774 \ub0a8\uc558\ub2e4. \uc774 \uad70\ub77d\uc740 \uc0ac\ub77c\uc9c0\uc9c0 \uc54a\ub294\ub2e4.";
        case Cache:
            return st.stage
                       ? L"\ucc59\uaca8\uac04 \ubb3c\uac74\uc758 \uc790\ub9ac\ub294 \ube44\uc5b4 \uc788\ub2e4. \ubd80\uc11c\uc9c4 \uc218\ub808\ub294 \ub204\uad70\uac00 \uc9c0\ub098\uac14\ub2e4\ub294 \ud754\uc801\uc73c\ub85c \ub0a8\ub294\ub2e4."
                       : L"\uc9c4\ud759 \uc18d\uc5d0 \uc218\ub808\uac00 \ubc84\ub824\uc838 \uc788\ub2e4. \ub9c8\ub978 \ubaa9\uc7ac\uc640 \uba39\uc744 \uc218 \uc788\ub294 \ube75\uc774 \ucc9c \uc544\ub798 \ub0a8\uc544 \uc788\ub2e4.";
        case Relic:
            return st.stage
                       ? L"\uc77d\uc5b4\ub0b8 \uae30\uc5b5\uc740 \uc774\ubbf8 \ub2f9\uc2e0\uc5d0\uac8c \uc62e\uaca8\uc654\ub2e4. \ud3d0\ud5c8\ub294 \uc5ec\uc804\ud788 \uc774 \uc790\ub9ac\uc5d0 \ub0a8\uc544 \uc788\ub2e4."
                       : L"\ub3cc \ud2c8\uc5d0 \ub204\uad70\uac00\uc758 \ub9c8\uc9c0\ub9c9 \uc21c\uac04\uc774 \ub0a8\uc544 \uc788\ub2e4. \ub9e8\uc190\uc73c\ub85c \ub9cc\uc9c0\uba74 \uc0dd\uba85\ub825\uc744 \uc783\ub294\ub2e4. \uc6b4\uba85 \uacc4\uc5f4\uc758 \uc9d5\uc870 \uc77d\uae30\ub85c \uc704\ud5d8\uc744 \ud53c\ud560 \uc218 \uc788\ub2e4.";
        default:
            return st.stage
                       ? L"\ub2f9\uc2e0\uc774 \ub0a8\uae34 \ucd1b\ubd88\uc774 \uc5b4\ub460 \uc18d\uc5d0\uc11c \ud0c0\uc624\ub978\ub2e4. \uc8fd\uc740 \uc790\ub4e4\uc758 \uc774\ub984\uc740 \uc544\uc9c1 \uc9c0\uc6cc\uc9c0\uc9c0 \uc54a\uc558\ub2e4."
                       : L"\ub3cc\uc5d0 \uc0c8\uae34 \uc774\ub984\ub4e4\uc774 \ub2f3\uc544 \uc788\ub2e4. \uae30\uc5b5\uc758 \ud30c\ud3b8 \ud558\ub098\ub97c \ubc14\uce58\uba74 \uc774\uacf3\uc5d0 \uc791\uc740 \ubd88\ube5b\uc744 \ub0a8\uae38 \uc218 \uc788\ub2e4.";
    }
}

void Act(int action)
{
    if (action == AffinityOpen && !InCombat() && g_Panel != 4)
    {
        g_Panel = g_Panel == 5 ? 0 : 5;
        g_Dialog = g_DeathConfirm = false;
        g_AffinityRefresh = -1;
        g_Buttons.clear();
        std::fill(std::begin(g_Keys), std::end(g_Keys), false);
        return;
    }
    if (g_Panel == 5)
    {
        if (action >= AffinitySelect)
        {
            g_AffinitySelected = action - AffinitySelect;
            return;
        }
        if (action >= AffinityPrevious && action <= AffinityMet)
        {
            if (action == AffinityPrevious) g_AffinityPage = std::max(0, g_AffinityPage - 1);
            if (action == AffinityNext) ++g_AffinityPage;
            if (action == AffinitySort) g_AffinityDescending = !g_AffinityDescending;
            if (action == AffinityVillage) g_AffinityVillage = (g_AffinityVillage + 2) % (Society::VillageCount + 1) - 1;
            if (action == AffinityMet) g_AffinityMetOnly = !g_AffinityMetOnly;
            if (action >= AffinitySort) g_AffinityPage = 0;
            g_AffinityRefresh = -1;
            return;
        }
    }
    if (action == AvatarOpen)
    {
        g_ProfileDraft = g_World.player.profile;
        g_ProfileField = 0;
        g_ProfileSelected = false;
        g_ProfileFacing = CharacterFacing::Front;
        g_ToastTime = 0;
        g_Panel = 4;
        g_Dialog = g_DeathConfirm = false;
        std::fill(std::begin(g_Keys), std::end(g_Keys), false);
        return;
    }
    if (g_Panel == 4)
    {
        if (action == AvatarCancel || action == Close)
        {
            g_Panel = 0;
            g_ProfileField = 0;
            return;
        }
        if (action == AvatarApply)
        {
            if (!ProfileValid(g_ProfileDraft))
            {
                Notify(L"\uc774\ub984\uc744 \uc785\ub825\ud574 \uc8fc\uc138\uc694.");
                return;
            }
            auto previous = g_World.player.profile;
            g_World.player.profile = g_ProfileDraft;
            g_World.dirty = true;
            if (!Persist())
            {
                g_World.player.profile = previous;
                return;
            }
            g_Panel = 0;
            g_ProfileField = 0;
            Notify(L"\ud504\ub85c\ud544\uc744 \uc800\uc7a5\ud588\uc2b5\ub2c8\ub2e4.");
            return;
        }
        if (action == AvatarName)
        {
            g_ProfileField = 1;
            g_ProfileSelected = true;
        }
        else if (action == AvatarMotto)
        {
            g_ProfileField = 2;
            g_ProfileSelected = true;
        }
        else if (action == AvatarTurn)
        {
            g_ProfileFacing = (CharacterFacing)(((unsigned)g_ProfileFacing + 1) % 4);
        }
        else
        {
            g_ProfileField = 0;
            if (action >= AvatarHairColor && action < AvatarHairColor + Art::Styles::HairCount)
            {
                g_ProfileDraft.style.hair = action - AvatarHairColor;
            }
            if (action >= AvatarSkin && action < AvatarSkin + Art::Styles::SkinCount)
            {
                g_ProfileDraft.style.skin = action - AvatarSkin;
            }
            if (action >= AvatarHairShape && action < AvatarHairShape + 5)
            {
                g_ProfileDraft.hairShape = 1 + action - AvatarHairShape;
            }
            if (action >= AvatarExpression && action < AvatarExpression + Art::Styles::ExpressionCount)
            {
                g_ProfileDraft.style.expression = action - AvatarExpression;
            }
            if (action >= AvatarOutfit && action < AvatarOutfit + 6)
            {
                g_ProfileDraft.outfit = 1 + action - AvatarOutfit;
            }
            if (action >= AvatarCloth && action <= AvatarCloth + Art::Styles::NpcClothCount)
            {
                g_ProfileDraft.style.cloth = action - AvatarCloth;
            }
        }
        return;
    }
    if (action == StoryOpen && !InCombat())
    {
        g_Panel = g_Panel == 3 ? 0 : 3;
        g_Dialog = g_DeathConfirm = false;
        std::fill(std::begin(g_Keys), std::end(g_Keys), false);
        return;
    }
    if (g_Panel == 3 && !InCombat())
    {
        if (action == StoryAdvance)
        {
            Story::Advance(g_World);
            Persist();
            return;
        }
        if (action == StoryBattle && Story::AvailableBattle(g_World))
        {
            Story::BeginBattle(g_World, NewLevelSeed());
            g_LevelView.reset(new LevelOneView(*g_Scene, g_World.storyBattle));
            g_Panel = 0;
            g_Buttons.clear();
            std::fill(std::begin(g_Keys), std::end(g_Keys), false);
            Persist();
            return;
        }
        if (action >= EndDestroy && action <= EndFreedom)
        {
            Story::ChooseEnding(g_World, action - EndDestroy + Story::Destroy);
            Persist();
            return;
        }
    }
    if (action == EnterLevel && !InCombat())
    {
        g_World.levelOne.active = true;
        g_LevelView.reset(new LevelOneView(*g_Scene, g_World.levelOne));
        g_LevelView->paused = false;
        g_Dialog = false;
        g_Panel = 0;
        std::fill(std::begin(g_Keys), std::end(g_Keys), false);
        Persist();
        return;
    }
    auto& p = g_World.player;
    if (action == CastInnate || action == CastAdjacent)
    {
        AbilityUse(action == CastAdjacent);
        return;
    }
    if (action == SelectMind || action == SelectMatter)
    {
        g_Secondary = action == SelectMind ? Mind : Matter;
        return;
    }
    if (action == Close)
    {
        g_Dialog = false;
        g_DeathConfirm = false;
        g_Panel = 0;
        return;
    }
    if (action == Journal || action == Abilities)
    {
        g_Panel = g_Panel == (action == Journal ? 1 : 2) ? 0 : (action == Journal ? 1 : 2);
        g_Dialog = false;
        g_DeathConfirm = false;
        return;
    }
    if (action == SaveNow)
    {
        if (Persist())
        {
            Notify(L"\uc138\uacc4\uc758 \ud754\uc801\uc744 \uc800\uc7a5\ud588\uc2b5\ub2c8\ub2e4.");
        }
        return;
    }
    if (action == Open)
    {
        g_NpcEconomy = false;
        RefreshTargets();
        if (g_TargetKind < 0)
        {
            return;
        }
        g_Dialog = true;
        g_Panel = 0;
        std::fill(std::begin(g_Keys), std::end(g_Keys), false);
        return;
    }
    if (!g_Dialog)
    {
        return;
    }
    bool allowed = false;
    for (const auto& c : Choices())
    {
        if (c.action == action && c.enabled)
        {
            allowed = true;
        }
    }
    if (!allowed)
    {
        return;
    }
    if (action == DeathQuestion)
    {
        g_DeathConfirm = true;
        return;
    }
    if (action == DeathAccept)
    {
        Die(g_World);
        g_Dialog = false;
        g_DeathConfirm = false;
        Notify(Story::DeathMessage(g_World));
        RefreshTargets();
        Persist();
        return;
    }
    if (g_TargetKind == 2)
    {
        auto& n = g_World.npcs[g_TargetIndex];
        if (action == NpcEconomy || action == NpcSocial)
        {
            g_NpcEconomy = action == NpcEconomy;
            return;
        }
        if (action == BuyFood || action == SellHerb)
        {
            Society::PlayerTrade(g_World, n, action == BuyFood);
            Persist();
            return;
        }
        if (action == AttackNpc)
        {
            Society::PlayerAttack(g_World, n.id);
            g_Dialog = false;
            RefreshTargets();
            Persist();
            return;
        }
        if (action == Talk)
        {
            Story::Talk(g_World, n);
            Notify(Story::NpcLine(g_World, n));
            g_World.Record(Remembered);
        }
        if (action == Share)
        {
            --p.bread;
            n.life.health = std::min(n.life.maxHealth, n.life.health + 12);
            Affinity::Apply(g_World, n, Affinity::Bread);
            p.echo[1] += 5;
            p.echo[6] += 2;
            g_World.Record(Befriended);
            Notify(L"\uadf8\uac00 \ub2f9\uc2e0 \uacc1\uc5d0 \uc870\uae08 \ub354 \uac00\uae4c\uc774 \uc120\ub2e4.");
        }
        if (action == TrainNpc)
        {
            Affinity::Apply(g_World, n, Affinity::Training);
            p.training[n.ability] = 1;
            g_Secondary = n.ability;
            p.stamina -= 25;
            g_World.Record(Learned);
            Notify(L"\uc778\uc811 \uacc4\uc5f4\uc758 \uc57d\ud55c \uae30\uc220\uc744 \uc775\ud614\uc2b5\ub2c8\ub2e4.");
        }
        if (action == RescueNpc)
        {
            --p.herbs;
            n.saved = true;
            n.life.health = n.life.maxHealth;
            Affinity::Apply(g_World, n, Affinity::Rescue);
            n.power = std::min(100, n.power + 10);
            p.echo[1] += 10;
            p.echo[4] += 6;
            Notify(
                L"\uc0c1\ucc98\uac00 \uc544\ubb3c\uc5c8\ub2e4. \uadf8\ub294 \uc624\ub298\uc758 \ub3c4\uc6c0\uc744 \uc790\uc2e0\uc758 \uae30\uc5b5\uc73c\ub85c \ub0a8\uae34\ub2e4."
            );
        }
        if (action == ChangeNpc)
        {
            p.wood -= 2;
            n.changed = n.freeWill = true;
            n.life.job = Society::Hunter;
            n.life.raidWeight = 0;
            n.life.target = 0;
            n.archetype = (n.archetype + 3) % 7;
            Affinity::Apply(g_World, n, Affinity::Support);
            n.power = std::min(100, n.power + 30);
            p.echo[6] += 12;
            Notify(
                L"\uacc4\uc2b9\uc790\ub294 \ubb3c\ub824\ubc1b\uc740 \ubc29\ud5a5\uacfc \ub2e4\ub978 \uc0b6\uc744 \uc120\ud0dd\ud588\ub2e4."
            );
        }
        if (action == ThreatenNpc)
        {
            Affinity::Apply(g_World, n, Affinity::Threat);
            p.echo[0] += 8;
            p.echo[5] += 4;
            Notify(
                L"\uadf8\ub294 \uace0\uac1c\ub97c \uc219\uc600\uc9c0\ub9cc, \ub2f9\uc2e0\uc744 \ud5a5\ud55c \uc801\uc758\ub294 \ub0a8\uc558\ub2e4."
            );
        }
    }
    else
    {
        const auto s = g_Sites[g_TargetIndex];
        auto& st = g_World.states[s.key];
        switch (action)
        {
            case StoryInvestigate:
                Notify(Story::Investigate(g_World, s));
                break;
            case Repair:
                p.wood -= 2;
                st.stage = 1;
                p.echo[6] += 8;
                g_World.Record(Repaired);
                Notify(L"\uc57c\uc601\uc9c0\uc5d0 \ubd88\uc774 \ub3cc\uc544\uc654\uc2b5\ub2c8\ub2e4.");
                break;
            case Rest:
                --p.bread;
                p.health = p.stamina = 100;
                p.checkpoint = s.p;
                p.echo[6] += 2;
                g_World.Record(Rested);
                Notify(
                    L"\uc774 \uc57c\uc601\uc9c0\uac00 \uc0c8\ub85c\uc6b4 \uc7ac\uc2dc\uc791 \uc9c0\uc810\uc774 \ub418\uc5c8\uc2b5\ub2c8\ub2e4."
                );
                break;
            case PickHerb:
                ++p.herbs;
                st.readyAt = g_World.time + 120;
                p.echo[3] += 2;
                g_World.Record(Gathered);
                Notify(
                    L"\uc57d\ucd08\ub97c \uc5bb\uc5c8\uc2b5\ub2c8\ub2e4. \ubfcc\ub9ac\ub294 \ub2e4\uc2dc \uc790\ub78d\ub2c8\ub2e4."
                );
                break;
            case TakeCache:
                p.wood += 2;
                ++p.bread;
                st.stage = 1;
                p.echo[3] += 3;
                g_World.Record(Salvaged);
                Notify(L"\ubaa9\uc7ac\uc640 \ube75\uc744 \ucc59\uacbc\uc2b5\ub2c8\ub2e4.");
                break;
            case HealHerb:
                --p.herbs;
                st.stage = 1;
                p.echo[1] += 12;
                p.echo[6] += 3;
                g_World.Record(Healed);
                Notify(L"\ud589\uc778\uc758 \ud638\ud761\uc774 \uace0\ub974\uac8c \ub3cc\uc544\uc654\uc2b5\ub2c8\ub2e4."
                );
                break;
            case CarryWounded:
                p.health -= 18;
                st.stage = 1;
                p.echo[4] += 14;
                p.echo[1] += 6;
                g_World.Record(Sacrificed);
                Notify(
                    L"\ubab8\uc744 \ub2e4\ucce4\uc9c0\ub9cc, \ud589\uc778\uc744 \uc548\uc804\ud558\uac8c \uad6c\uc870\ud588\uc2b5\ub2c8\ub2e4."
                );
                break;
            case Rob:
                p.bread += 2;
                st.stage = 2;
                p.echo[5] += 12;
                p.echo[0] += 6;
                g_World.Record(Robbed);
                Notify(L"\ud589\uc778\uc740 \ub2f9\uc2e0\uc5d0\uac8c\uc11c \ub208\uc744 \ub3cc\ub838\uc2b5\ub2c8\ub2e4."
                );
                break;
            case ReadRelic:
                p.health -= 16;
                st.stage = 1;
                ++p.fragments;
                p.echo[3] += 8;
                p.echo[2] += 3;
                g_World.Record(Investigated);
                Notify(
                    L"\uc624\ub798\ub41c \uae30\uc5b5\uc774 \uc0c1\ucc98\uc640 \ud568\uaed8 \ub0a8\uc558\uc2b5\ub2c8\ub2e4."
                );
                break;
            case ReadFate:
                p.stamina -= 35;
                st.stage = 1;
                ++p.fragments;
                p.echo[3] += 8;
                g_World.Record(Investigated);
                Notify(
                    L"\uc9d5\uc870\ub97c \ub530\ub77c \uc548\uc804\ud558\uac8c \uae30\uc5b5\uc744 \uc77d\uc5c8\uc2b5\ub2c8\ub2e4."
                );
                break;
            case Offer:
                --p.fragments;
                st.stage = 1;
                p.echo[4] += 5;
                p.echo[6] += 10;
                g_World.Record(Offered);
                Notify(
                    L"\uc774\ub984 \uc5c6\ub294 \uc790\ub4e4\uc744 \uc704\ud55c \ucd1b\ubd88\uc774 \ucf1c\uc84c\uc2b5\ub2c8\ub2e4."
                );
                break;
            case Train:
                --p.bread;
                p.stamina -= 25;
                p.training[s.ability] = 1;
                g_Secondary = s.ability;
                st.trained = true;
                g_World.Record(Learned);
                Notify(
                    L"\uc21c\ub840\uc790\uac00 \uc790\uc2e0\uc758 \uc791\uc740 \uae30\uc220\uc744 \ub098\ub234\uc2b5\ub2c8\ub2e4."
                );
                break;
        }
    }
    if (p.health <= 0)
    {
        Die(g_World, Story::Ruin);
        Notify(Story::DeathMessage(g_World));
    }
    g_Dialog = false;
    g_DeathConfirm = false;
    RefreshTargets();
    Persist();
}

void AbilityUse(bool secondary)
{
    if (g_Dialog || g_Panel)
    {
        return;
    }
    auto& p = g_World.player;
    if (p.stamina < 25)
    {
        Notify(L"\uae30\ub825\uc774 \ubd80\uc871\ud569\ub2c8\ub2e4.");
        return;
    }
    if (!secondary)
    {
        p.stamina -= 25;
        double best = 1e20;
        Site nearest = GetSite(0, 0);
        for (const auto& s : g_Sites)
        {
            auto st = State(g_World, s.key);
            if ((s.kind == Relic || s.kind == Cache) && st.stage == 0 && Distance(s.p, p.p) < best)
            {
                best = Distance(s.p, p.p);
                nearest = s;
            }
        }
        if (best < 1e19)
        {
            auto q = g_Scene->Project(nearest.p);
            std::wstring direction =
                std::abs(q.x - g_Width * .5) > std::abs(q.y - g_Height * IsometricView::AnchorY)
                    ? (q.x > g_Width * .5 ? L"\ub3d9\ucabd" : L"\uc11c\ucabd")
                    : (q.y > g_Height * IsometricView::AnchorY ? L"\ub0a8\ucabd" : L"\ubd81\ucabd");
            Notify(
                direction +
                L"\uc5d0\uc11c \uc544\uc9c1 \uc0ac\ub77c\uc9c0\uc9c0 \uc54a\uc740 \ud754\uc801\uc774 \ub290\uaef4\uc9d1\ub2c8\ub2e4."
            );
        }
        else
        {
            Notify(
                L"\uac00\uae4c\uc6b4 \uacf3\uc5d0\uc11c\ub294 \uc0c8\ub85c\uc6b4 \uc9d5\uc870\uac00 \ub290\uaef4\uc9c0\uc9c0 \uc54a\uc2b5\ub2c8\ub2e4."
            );
        }
    }
    else if (g_Secondary == Matter && p.training[Matter])
    {
        RefreshTargets();
        if (g_TargetKind != 1 || g_Sites[g_TargetIndex].kind != Camp ||
            State(g_World, g_Sites[g_TargetIndex].key).stage)
        {
            Notify(
                L"\ubd88\uc774 \uaebc\uc9c4 \uc57c\uc601\uc9c0 \uac00\uae4c\uc774\uc5d0\uc11c \ubd88\uc528\ub97c \ub2e4\ub8f0 \uc218 \uc788\uc2b5\ub2c8\ub2e4."
            );
            return;
        }
        if (p.wood < 1)
        {
            Notify(L"\uc57d\ud55c \ubd88\uc528\uc5d0\ub3c4 \ubaa9\uc7ac 1\uac1c\ub294 \ud544\uc694\ud569\ub2c8\ub2e4.");
            return;
        }
        --p.wood;
        p.stamina -= 25;
        g_World.states[g_Sites[g_TargetIndex].key].stage = 1;
        p.echo[6] += 5;
        g_World.Record(Repaired);
        Notify(L"\uc57d\ud55c \ubb3c\uc9c8 \ub2a5\ub825\uc73c\ub85c \ubd88\uc528\ub97c \uc0b4\ub838\uc2b5\ub2c8\ub2e4."
        );
    }
    else if (g_Secondary == Mind && p.training[Mind])
    {
        p.stamina -= 25;
        p.health = std::min(100.f, p.health + 5);
        Notify(
            L"\ud638\ud761\uc744 \uac00\ub2e4\ub4ec\uc5b4 \uc791\uc740 \uc0c1\ucc98\ub97c \ub2e4\uc2a4\ub838\uc2b5\ub2c8\ub2e4."
        );
    }
    else
    {
        Notify(
            L"\uc544\uc9c1 \uc778\uc811 \uacc4\uc5f4 \ub2a5\ub825\uc744 \ubc30\uc6b0\uc9c0 \uc54a\uc558\uc2b5\ub2c8\ub2e4."
        );
        return;
    }
    g_World.dirty = true;
    Persist();
}

float Wrap(float x, float y, float maxWidth, const std::wstring& value, Color color, int size = 16)
{
    std::wstring line;
    float width = 0;
    for (wchar_t c : value)
    {
        float step = c >= 0x2e80 ? (float)size : size * .61f;
        if (c == L'\n' || width + step > maxWidth)
        {
            Text(x, y, line, color, size);
            y += size + 9;
            line.clear();
            width = 0;
            if (c == L'\n')
            {
                continue;
            }
        }
        line += c;
        width += step;
    }
    if (!line.empty())
    {
        Text(x, y, line, color, size);
        y += size + 9;
    }
    return y;
}

bool Hover(float x, float y, float w, float h)
{
    float mx = g_MouseX / g_UiScale, my = g_MouseY / g_UiScale;
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

void AddButton(float x, float y, float w, float h, const std::wstring& label, int action, bool enabled = true)
{
    g_Buttons.push_back({x, y, w, h, action, enabled, label});
    Box(x,
        y,
        w,
        h,
        enabled ? (Hover(x, y, w, h) ? Color(.27f, .32f, .30f, .98f) : Color(.19f, .23f, .22f, .96f))
                : Color(.13f, .16f, .15f, .96f));
    Line(x, y + h, x + w, y + h, 1, enabled ? Color(.44f, .48f, .42f) : Color(.24f, .27f, .25f));
    Text(x + 14, y + (h - 22) * .5f, label, enabled ? g_Ink : Color(.43f, .47f, .44f), 15);
}

void IconButton(float x, float y, int action, const std::wstring& label)
{
    g_Buttons.push_back({x, y, 40, 40, action, true, label});
    if (Hover(x, y, 40, 40))
    {
        Box(x, y, 40, 40, Color(.27f, .31f, .28f));
    }
    Color c = g_Muted;
    if (action == SaveNow)
    {
        Line(x + 11, y + 10, x + 29, y + 10, 1.5f, c);
        Line(x + 11, y + 10, x + 11, y + 30, 1.5f, c);
        Line(x + 29, y + 10, x + 29, y + 30, 1.5f, c);
        Line(x + 11, y + 30, x + 29, y + 30, 1.5f, c);
        Box(x + 15, y + 11, 10, 6, c);
        Line(x + 16, y + 23, x + 24, y + 23, 1, c);
    }
    else if (action == Journal)
    {
        Line(x + 10, y + 11, x + 10, y + 29, 1.5f, c);
        Line(x + 30, y + 11, x + 30, y + 29, 1.5f, c);
        Line(x + 10, y + 11, x + 20, y + 14, 1.5f, c);
        Line(x + 20, y + 14, x + 30, y + 11, 1.5f, c);
        Line(x + 10, y + 29, x + 20, y + 32, 1.5f, c);
        Line(x + 20, y + 32, x + 30, y + 29, 1.5f, c);
        Line(x + 20, y + 14, x + 20, y + 32, 1.5f, c);
    }
    else
    {
        Line(x + 20, y + 8, x + 30, y + 20, 1.5f, c);
        Line(x + 30, y + 20, x + 20, y + 32, 1.5f, c);
        Line(x + 20, y + 32, x + 10, y + 20, 1.5f, c);
        Line(x + 10, y + 20, x + 20, y + 8, 1.5f, c);
    }
    if (Hover(x, y, 40, 40))
    {
        Box(x - 18, y - 32, 76, 26, Color(.07f, .10f, .09f, .98f));
        Text(x - 10, y - 30, label, g_Ink, 14);
    }
}

void Bar(float x, float y, float w, float value, Color c)
{
    Box(x, y, w, 5, Color(.08f, .11f, .10f));
    Box(x, y, w * std::max(0.f, std::min(value, 100.f)) / 100, 5, c);
}

void Map()
{
    float x = UiWidth() - 216, y = 106, size = 192;
    Box(x, y, size, size, Color(.065f, .09f, .08f, .94f));
    for (int iy = 0; iy < 48; ++iy)
    {
        for (int ix = 0; ix < 48; ++ix)
        {
            double sx = (ix - 24) * 12.0, sy = (iy - 24) * 12.0;
            auto ground = IsometricView::GroundFromScreen({sx, sy});
            Vec p(g_World.player.p.x + ground.x, g_World.player.p.y + ground.y);
            Box(x + ix * 4, y + iy * 4, 4, 4, Road(p) < 33 ? Color(.39f, .40f, .33f) : Color(.16f, .21f, .18f));
        }
    }
    auto mark = [&](Vec p, Color c, float radius)
    {
        double dx = p.x - g_World.player.p.x, dy = p.y - g_World.player.p.y;
        auto projected = IsometricView::Project({dx, dy, 0});
        float mx = (float)(projected.x / 3), my = (float)(projected.y / 3);
        if (std::abs(mx) < 90 && std::abs(my) < 90)
        {
            g_Renderer->Ellipse(
                (x + 96 + mx) * g_UiScale,
                (y + 96 + my) * g_UiScale,
                radius * g_UiScale,
                radius * g_UiScale,
                c,
                c
            );
        }
    };
    for (const auto& s : g_Sites)
    {
        mark(s.p, s.kind == Wounded ? Color(.43f, .74f, .65f) : g_Accent, 2.5f);
    }
    for (const auto& n : g_World.npcs)
    {
        if (n.life.alive)
        {
            mark(n.p, n.life.target ? Color(.82f, .36f, .28f) : Color(.58f, .74f, .72f), 2.5f);
        }
    }
    for (int village = 0; village < Society::VillageCount; ++village)
    {
        mark(VillageCenter(village), Color(.88f, .81f, .52f), 5);
    }
    mark(g_World.player.p, g_Ink, 3.5f);
    auto north = IsometricView::Project({0, 1, 0});
    double northLength = std::hypot(north.x, north.y);
    float nx = (float)(north.x / northLength), ny = (float)(north.y / northLength);
    Line(x + 26, y + 28, x + 26 + nx * 12, y + 28 + ny * 12, 1.5f, g_Ink);
    Text(x + 22 + nx * 20, y + 20 + ny * 20, L"N", g_Ink, 13);
    Text(
        x,
        y + size + 7,
        L"\uc7ac\uc758 \uae38  \u00b7  " + std::to_wstring(Chunk(g_World.player.p.x)) + L", " +
            std::to_wstring(Chunk(g_World.player.p.y)),
        g_Muted,
        14
    );
}

void SidePanel()
{
    float x = UiWidth() - 372, y = 104, w = 348, bottom = UiHeight() - 100;
    Box(x, y, w, bottom - y, Color(.065f, .09f, .085f, .98f));
    Text(x + 20, y + 18, g_Panel == 1 ? L"\ub0a8\uaca8\uc9c4 \uae30\ub85d" : L"\uace0\uc720\ub2a5\ub825", g_Ink, 21);
    g_Buttons.push_back({x + w - 44, y + 12, 32, 32, Close, true, L"\ub2eb\uae30"});
    Line(x + w - 34, y + 22, x + w - 22, y + 34, 1.4f, g_Muted);
    Line(x + w - 22, y + 22, x + w - 34, y + 34, 1.4f, g_Muted);
    if (g_Panel == 1)
    {
        float row = y + 68;
        int offset = std::min(g_JournalOffset, std::max(0, (int)g_World.events.size() - 1));
        for (auto i = g_World.events.rbegin() + offset; i != g_World.events.rend() && row + 64 < bottom; ++i)
        {
            row = Wrap(x + 20, row, w - 40, EventText(*i), g_Muted, 15) + 16;
            Line(x + 20, row - 8, x + w - 20, row - 8, 1, Color(.20f, .25f, .22f));
        }
    }
    else
    {
        auto& p = g_World.player;
        float row = y + 62;
        Text(x + 20, row, std::wstring(L"\uc120\ucc9c \uacc4\uc5f4  \u00b7  ") + AbilityName(p.ability), g_Accent, 18);
        row =
            Wrap(
                x + 20,
                row + 32,
                w - 40,
                L"\uc9d5\uc870 \uc77d\uae30: \ubcf4\uae09\ud488\uacfc \uae30\uc5b5\uc758 \ubc29\ud5a5\uc744 \uac10\uc9c0\ud55c\ub2e4. \uae30\ub825 25.",
                g_Muted,
                14
            ) +
            10;
        for (int i = 0; i < AbilityCount; ++i)
        {
            bool innate = i == p.ability, adjacent = Adjacent(p.ability, (Ability)i);
            std::wstring state = innate          ? L"\uc120\ucc9c"
                                 : p.training[i] ? L"\uc57d\ud55c \ub2a5\ub825 \uc2b5\ub4dd"
                                 : adjacent      ? L"\uc218\ub828 \uac00\ub2a5"
                                                 : L"\uba3c \uacc4\uc5f4";
            if (p.training[i])
            {
                g_Buttons.push_back({x + 12, row - 2, w - 24, 26, i == Mind ? SelectMind : SelectMatter, true, L""});
                if (i == g_Secondary)
                {
                    Box(x + 12, row - 2, w - 24, 26, Color(.22f, .29f, .24f));
                }
            }
            Text(x + 20, row, AbilityName((Ability)i), innate ? g_Accent : g_Muted, 16);
            Text(x + 132, row, state, adjacent || innate ? g_Ink : g_Muted, 14);
            row += 28;
        }
        if (p.training[Matter])
        {
            row = Wrap(
                x + 20,
                row + 8,
                w - 40,
                L"\uc791\uc740 \ubd88\uc528: \ubaa9\uc7ac 1, \uae30\ub825 25\ub85c \uc57c\uc601\uc9c0\ub97c \ubcf5\uad6c\ud55c\ub2e4.",
                g_Muted,
                13
            );
        }
        if (p.training[Mind])
        {
            Wrap(
                x + 20,
                row + 4,
                w - 40,
                L"\uace0\uc694\ud55c \ud638\ud761: \uae30\ub825 25\ub85c \uccb4\ub825 5\ub97c \ud68c\ubcf5\ud55c\ub2e4.",
                g_Muted,
                13
            );
        }
    }
}

void StoryPanel()
{
    g_Buttons.clear();
    float w = 680, x = (UiWidth() - w) * .5f, y = 104;
    float h = UiHeight() - 200;
    Box(0, 88, UiWidth(), UiHeight() - 164, Color(.02f, .03f, .025f, .82f));
    Box(x, y, w, h, Color(.075f, .10f, .095f, .99f));
    auto& story = g_World.story;
    Text(x + 24, y + 14, Story::QuestId(story.stage), g_Accent, 13);
    Text(x + 24, y + 38, Story::Title(story.stage), g_Ink, 21);
    float row = Wrap(x + 24, y + 76, w - 48, Story::Passage(g_World), g_Muted, 15) + 12;
    row = Wrap(x + 24, row, w - 48, Story::Objective(g_World), g_Ink, 15) + 10;
    if (story.stage == Story::Ruins)
    {
        for (int i = 0; i < AbilityCount; ++i)
        {
            auto key = Story::RuinKeys()[i];
            auto site = GetSite(key.first, key.second);
            std::wstring label = std::wstring(AbilityName((Ability)i)) +
                                 ((story.relics & (1 << i)) ? L" \u00b7 \ubc1c\uacac"
                                                            : L" \u00b7 " + std::to_wstring((int)site.p.x) + L", " +
                                                                  std::to_wstring((int)site.p.y));
            Text(x + 24 + (i % 2) * 312, row + (i / 2) * 25, label, g_Accent, 14);
        }
    }
    if (story.stage == Story::Successors)
    {
        int shown = 0;
        for (const auto& n : g_World.npcs)
        {
            if (n.origin == 0 && !n.met && shown < 3)
            {
                Text(
                    x + 24,
                    row + shown * 24,
                    std::to_wstring(n.id) + L"\ubc88\uc9f8 \uacc4\uc2b9\uc790 \u00b7 " + std::to_wstring((int)n.p.x) +
                        L", " + std::to_wstring((int)n.p.y),
                    g_Accent,
                    14
                );
                ++shown;
            }
        }
    }
    if (story.stage == Story::ShapedWorld || story.stage == Story::FirstGrave)
    {
        Wrap(
            x + 24,
            row,
            w - 48,
            L"\ud568\uaed8\ud560 \uacc4\uc2b9\uc790 " + std::to_wstring(Story::Count(g_World, 4)) +
                L" \u00b7 \uc801\ub300\ud558\ub294 \uacc4\uc2b9\uc790 " + std::to_wstring(Story::Count(g_World, 5)) +
                L" \u00b7 \uae30\ub85d\ub41c \uc8fd\uc74c " + std::to_wstring(story.deaths.size()),
            g_Accent,
            14
        );
    }
    if (story.stage == Story::EndingChoice)
    {
        Wrap(
            x + 24,
            row,
            w - 48,
            Story::FreedomReady(g_World)
                ? L"\uc544\uc774\uc640 \uacc4\uc2b9\uc790\ub4e4\uc774 \uac01\uc790\uc758 \uc0b6\uc744 \uace0\ub97c \uc900\ube44\uac00 \ub418\uc5c8\ub2e4."
                : L"\ud574\ubc29\uc758 \uc870\uac74: \uacc4\uc2b9\uc790 10\uba85, \ubcf4\ud638 5\uba85, \ub2e4\ub978 \uc9c4\ub85c 3\uba85, \uc544\uc774\uc5d0\uac8c \uc9c4\uc2e4 \uc804\ub2ec, \uce58\uc6b0\uce58\uc9c0 \uc54a\uc740 \uc0b6.",
            g_Accent,
            13
        );
        float by = y + h - 158;
        AddButton(x + 24, by, w - 48, 40, L"\uacc4\uc2b9\uc758 \uc885\ub9d0", EndDestroy);
        AddButton(x + 24, by + 48, w - 48, 40, L"\uc0c8\ub85c\uc6b4 \uacc4\uc2b9\uc790", EndSuccessor);
        AddButton(
            x + 24,
            by + 96,
            w - 48,
            40,
            L"\uacc4\uc2b9\uc758 \ud574\ubc29",
            EndFreedom,
            Story::FreedomReady(g_World)
        );
    }
    else if (Story::AvailableBattle(g_World))
    {
        const wchar_t* label =
            story.stage == Story::Prologue ? L"\ub9c8\uc744\uc744 \ub36e\uce5c \uc2b5\uaca9\uc5d0 \ub9de\uc120\ub2e4"
            : story.stage == Story::Abelon ? L"\ubd89\uc740 \uc608\ubc30\ub2f9\uc73c\ub85c \uac04\ub2e4"
                                           : L"\ud0dc\ucd08\uc758 \ubb18\uc9c0\ub85c \ub4e4\uc5b4\uac04\ub2e4";
        AddButton(x + 24, y + h - 58, w - 48, 40, label, StoryBattle);
    }
    else if (Story::CanAdvance(g_World))
    {
        AddButton(
            x + 24,
            y + h - 58,
            w - 48,
            40,
            L"\ub0a8\uaca8\uc9c4 \uae30\ub85d\uc744 \ub530\ub77c\uac04\ub2e4",
            StoryAdvance
        );
    }
    g_Buttons.push_back({x + w - 44, y + 12, 32, 32, Close, true, L"\ub2eb\uae30"});
    Line(x + w - 34, y + 22, x + w - 22, y + 34, 1.4f, g_Muted);
    Line(x + w - 22, y + 22, x + w - 34, y + 34, 1.4f, g_Muted);
}

void AvatarPanel()
{
    g_UiScale = std::min(1.f, std::min(g_Width / 960.f, g_Height / 640.f));
    g_Buttons.clear();
    float w = 900, h = 580, x = (UiWidth() - w) * .5f, y = (UiHeight() - h) * .5f;
    Box(0, 0, UiWidth(), UiHeight(), Color(.015f, .025f, .025f, .88f));
    Box(x, y, w, h, Color(.085f, .11f, .105f, .99f));
    Text(x + 24, y + 18, L"\ud504\ub85c\ud544 \u00b7 \uc544\ubc14\ud0c0", g_Ink, 21);
    Line(x + 270, y + 64, x + 270, y + h - 76, 1, Color(.26f, .31f, .28f));
    Art::DrawRequest preview;
    preview.assets = Art::Assets().PlayerRecipe(g_ProfileDraft.hairShape, g_ProfileDraft.outfit);
    preview.style = g_ProfileDraft.style;
    preview.clip = "Idle";
    preview.seconds = glfwGetTime();
    preview.yaw = Art::FacingYaw(g_ProfileFacing);
    preview.x = (x + 142) * g_UiScale;
    preview.y = (y + 335) * g_UiScale;
    preview.scale = 3 * g_UiScale;
    preview.deferAnimation = true;
    preview.preciseDepth = true;
    preview.animationStream = std::uint64_t(1) << 63;
    g_Renderer->Ellipse(
        preview.x,
        preview.y + 6 * g_UiScale,
        74 * g_UiScale,
        15 * g_UiScale,
        Color(.02f, .03f, .025f, .8f),
        Color(.02f, .03f, .025f, 0)
    );
    if (!Art::Draw(*g_Renderer, preview))
    {
        Text(x + 36, y + 250, L"\ubaa8\ub378 \ub85c\ub529 \ub300\uae30", g_Muted, 15);
    }
    AddButton(x + 89, y + 361, 106, 36, L"\ud68c\uc804", AvatarTurn);
    Wrap(x + 28, y + 417, 220, g_ProfileDraft.name, g_Ink, 13);
    Wrap(x + 28, y + 451, 220, g_ProfileDraft.motto, g_Muted, 13);
    float cx = x + 294, cw = 578;
    auto field = [&](float top, const wchar_t* title, const std::wstring& text, int action, int active, float height)
    {
        Text(cx, top, title, g_Muted, 13);
        Box(cx, top + 20, cw, height, Color(.045f, .065f, .06f));
        if (active == g_ProfileField && g_ProfileSelected)
        {
            Box(cx + 6, top + 24, cw - 12, height - 8, Color(.20f, .31f, .29f));
        }
        Line(
            cx,
            top + 20 + height,
            cx + cw,
            top + 20 + height,
            active == g_ProfileField ? 2.f : 1.f,
            active == g_ProfileField ? g_Accent : g_Muted
        );
        Wrap(cx + 10, top + 25, cw - 20, text + (active == g_ProfileField ? L"|" : L""), g_Ink, 15);
        g_Buttons.push_back({cx, top + 20, cw, height, action, true, title});
    };
    field(y + 62, L"\uc774\ub984", g_ProfileDraft.name, AvatarName, 1, 32);
    field(y + 122, L"\ud55c\ub9c8\ub514", g_ProfileDraft.motto, AvatarMotto, 2, 36);
    Text(cx, y + 189, std::wstring(L"\uc131\ubcc4  ") + PlayerProfile::Gender() + L"  (\uace0\uc815)", g_Muted, 14);
    auto swatches = [&](float top, const wchar_t* title, unsigned count, int base, unsigned selected, int role)
    {
        Text(cx, top + 6, title, g_Muted, 13);
        for (unsigned i = 0; i < count; ++i)
        {
            auto style = g_ProfileDraft.style;
            if (role == 0)
            {
                style.skin = i;
            }
            if (role == 1)
            {
                style.hair = i;
            }
            if (role == 2)
            {
                style.cloth = i;
            }
            const auto rgb = Art::Styles::Palette(style)[role];
            float sx = cx + 96 + i * 34.f;
            Box(sx,
                top,
                26,
                26,
                Color(std::pow(rgb[0], 1 / 2.2f), std::pow(rgb[1], 1 / 2.2f), std::pow(rgb[2], 1 / 2.2f)));
            if (i == selected)
            {
                Line(sx - 2, top - 2, sx + 28, top - 2, 2, g_Accent);
                Line(sx - 2, top + 28, sx + 28, top + 28, 2, g_Accent);
                Line(sx - 2, top - 2, sx - 2, top + 28, 2, g_Accent);
                Line(sx + 28, top - 2, sx + 28, top + 28, 2, g_Accent);
            }
            g_Buttons.push_back({sx, top, 26, 26, base + (int)i, true, L""});
        }
    };
    swatches(y + 224, L"\uba38\ub9ac\uc0c9", Art::Styles::HairCount, AvatarHairColor, g_ProfileDraft.style.hair, 1);
    swatches(y + 265, L"\ud53c\ubd80\uc0c9", Art::Styles::SkinCount, AvatarSkin, g_ProfileDraft.style.skin, 0);
    auto options =
        [&](float top, const wchar_t* title, const std::vector<std::wstring>& labels, int base, unsigned selected)
    {
        Text(cx, top, title, g_Muted, 13);
        float width = (cw - (labels.size() - 1) * 6) / labels.size();
        for (size_t i = 0; i < labels.size(); ++i)
        {
            float left = cx + i * (width + 6);
            AddButton(left, top + 20, width, 32, labels[i], base + (int)i);
            if (i == selected)
            {
                Line(left, top + 52, left + width, top + 52, 2, g_Accent);
            }
        }
    };
    options(
        y + 309,
        L"\ud5e4\uc5b4",
        {L"\uc20f\ucef7", L"\ub2e8\ubc1c", L"\uae34 \uba38\ub9ac", L"\ub54b\uae30", L"\ub118\uae40"},
        AvatarHairShape,
        g_ProfileDraft.hairShape - 1
    );
    options(
        y + 370,
        L"\ud45c\uc815",
        {L"\uc628\ud654", L"\ud3c9\uc628", L"\uacb0\uc758", L"\uac71\uc815", L"\uc878\ub9bc", L"\ubbf8\uc18c"},
        AvatarExpression,
        g_ProfileDraft.style.expression
    );
    options(
        y + 431,
        L"\uc758\uc0c1",
        {L"\ud29c\ub2c9", L"\ud310\uae08", L"\uad81\uc218", L"\ub85c\ube0c", L"\uc0c1\uc778", L"\uce58\uc720\uc0ac"},
        AvatarOutfit,
        g_ProfileDraft.outfit - 1
    );
    swatches(y + 491, L"\uc637 \uc0c9", Art::Styles::NpcClothCount + 1, AvatarCloth, g_ProfileDraft.style.cloth, 2);
    AddButton(x + w - 232, y + h - 46, 100, 32, L"\ucde8\uc18c", AvatarCancel);
    AddButton(x + w - 122, y + h - 46, 100, 32, L"\uc801\uc6a9", AvatarApply, ProfileValid(g_ProfileDraft));
    if (g_ToastTime > 0)
    {
        Wrap(x + 24, y + h - 46, 620, g_Toast, g_Accent, 13);
    }
}

Color AffinityColor(int score)
{
    return score < 0 ? Color(.93f, .53f, .49f) : score >= 25 ? Color(.49f, .82f, .67f) : g_Muted;
}

void AffinityPanel()
{
    const float w = 900, h = std::min(720.f, UiHeight() - 64);
    const float x = (UiWidth() - w) * .5f, y = (UiHeight() - h) * .5f;
    const int pageSize = std::max(1, (int)((h - 212) / 48));
    if (glfwGetTime() >= g_AffinityRefresh || g_AffinityRevision != g_World.affinity.revision ||
        g_AffinityPopulation != g_World.npcs.size())
    {
        g_AffinityRanking = Affinity::Ranking(g_World, g_AffinityVillage, g_AffinityMetOnly, g_AffinityDescending);
        g_AffinityRefresh = glfwGetTime() + 1;
        g_AffinityRevision = g_World.affinity.revision;
        g_AffinityPopulation = g_World.npcs.size();
    }
    int pages = ((int)g_AffinityRanking.size() + pageSize - 1) / pageSize;
    g_AffinityPage = std::max(0, std::min(g_AffinityPage, pages - 1));
    const Npc* selected = nullptr;
    for (auto i : g_AffinityRanking)
        if (g_World.npcs[i].id == g_AffinitySelected) { selected = &g_World.npcs[i]; break; }
    if (!selected && !g_AffinityRanking.empty())
    {
        selected = &g_World.npcs[g_AffinityRanking[g_AffinityPage * pageSize]];
        g_AffinitySelected = selected->id;
    }

    g_Buttons.clear();
    Box(0, 0, UiWidth(), UiHeight(), Color(.01f, .025f, .02f, .78f));
    Box(x, y, w, h, Color(.065f, .09f, .085f, .99f));
    Line(x, y, x + w, y, 2, g_Accent);
    Text(x + 24, y + 20, L"\ud638\uac10\ub3c4 \uc21c\uc704", g_Ink, 24);
    Text(x + 24, y + 56, L"\uc8fc\ubbfc\uc774 " + g_World.player.profile.name + L"\uc5d0\uac8c \ub290\ub07c\ub294 \ub9c8\uc74c  \u00b7  -100 ~ +100", g_Muted, 14);
    AddButton(x + w - 110, y + 18, 86, 32, L"\ub2eb\uae30  Esc", Close);
    AddButton(x + 24, y + 90, 146, 32, g_AffinityDescending ? L"\ub192\uc740 \uc21c \u2193" : L"\ub0ae\uc740 \uc21c \u2191", AffinitySort);
    AddButton(x + 180, y + 90, 190, 32,
              g_AffinityVillage < 0 ? L"\ubaa8\ub4e0 \ub9c8\uc744" : Society::VillageName(g_AffinityVillage), AffinityVillage);
    AddButton(x + 380, y + 90, 148, 32, g_AffinityMetOnly ? L"\ub300\ud654\ud55c NPC" : L"\ubaa8\ub4e0 \uc0dd\uc874 NPC", AffinityMet);
    Text(x + 24, y + 134, L"\uc21c\uc11c   \uc774\ub984 / \ub9c8\uc744", g_Muted, 12);
    Text(x + 330, y + 134, L"\ud638\uac10\ub3c4 / \uad00\uacc4 \ub2e8\uacc4", g_Muted, 12);
    for (int row = 0; row < pageSize; ++row)
    {
        int index = g_AffinityPage * pageSize + row;
        if (index >= (int)g_AffinityRanking.size()) break;
        const auto& n = g_World.npcs[g_AffinityRanking[index]];
        float ry = y + 158 + row * 48.f;
        Box(x + 18, ry, 516, 44, n.id == g_AffinitySelected ? Color(.14f, .23f, .20f) : Color(.09f, .13f, .12f));
        Text(x + 28, ry + 10, std::to_wstring(index + 1), g_Accent, 16);
        Text(x + 68, ry + 3, n.Name() + L"  #" + std::to_wstring(n.id), g_Ink, 15);
        Text(x + 68, ry + 24, std::wstring(Society::VillageName(n.life.village)) + L" \u00b7 " + Society::JobName(n.life.job), g_Muted, 11);
        Text(x + 330, ry + 4, Affinity::Signed(n.relationship) + L"  " + Affinity::Tier(n.relationship), AffinityColor(n.relationship), 14);
        Box(x + 330, ry + 31, 176, 4, Color(.19f, .24f, .22f));
        float fill = n.relationship * .88f;
        Box(x + 418 + std::min(0.f, fill), ry + 31, std::abs(fill), 4, AffinityColor(n.relationship));
        Line(x + 418, ry + 29, x + 418, ry + 37, 1, g_Muted);
        g_Buttons.push_back({x + 18, ry, 516, 44, AffinitySelect + n.id, true, n.Name()});
    }
    if (g_AffinityRanking.empty()) Text(x + 40, y + 200, L"\uc870\uac74\uc5d0 \ub9de\ub294 \uc0dd\uc874 NPC\uac00 \uc5c6\uc2b5\ub2c8\ub2e4.", g_Muted, 16);
    Text(x + 24, y + h - 36, std::to_wstring(g_AffinityRanking.size()) + L"\uba85 \u00b7 \ub3d9\uc810\uc740 ID \uc21c", g_Muted, 12);
    Text(x + 285, y + h - 36, std::to_wstring(pages ? g_AffinityPage + 1 : 0) + L" / " + std::to_wstring(pages), g_Ink, 14);
    AddButton(x + 374, y + h - 46, 72, 30, L"\uc774\uc804", AffinityPrevious, g_AffinityPage > 0);
    AddButton(x + 456, y + h - 46, 72, 30, L"\ub2e4\uc74c", AffinityNext, g_AffinityPage + 1 < pages);
    Line(x + 550, y + 90, x + 550, y + h - 22, 1, Color(.24f, .30f, .27f));
    float dx = x + 574;
    if (selected)
    {
        const auto& n = *selected;
        Text(dx, y + 96, n.Name() + L"  #" + std::to_wstring(n.id), g_Ink, 20);
        Text(dx, y + 130, Affinity::Signed(n.relationship) + L"  " + Affinity::Tier(n.relationship), AffinityColor(n.relationship), 24);
        Text(dx, y + 169, L"\uc2e0\ub8b0 " + std::to_wstring(n.trust) + L" / 3  \u00b7  " + (n.met ? L"\ub300\ud654\ud55c \uc0ac\uc774" : L"\uc544\uc9c1 \ub300\ud654 \uc804"), g_Muted, 13);
        double wait = Affinity::Remaining(g_World.society.elapsed, n.affinity.lastTalk, Affinity::TalkCooldown);
        Text(dx, y + 194, n.relationship >= 100 ? L"\ucd5c\ub300 \ud638\uac10\ub3c4" : wait > 1e-8 ? L"\ub300\ud654 \ubcf4\uc0c1\uae4c\uc9c0 " + std::to_wstring((int)std::ceil(wait)) + L"\ucd08" : L"\ub300\ud654 \ud638\uac10\ub3c4 +5 \uac00\ub2a5", g_Accent, 13);
        Text(dx, y + 230, L"\ucd5c\uadfc \ubcc0\ud654", g_Ink, 16);
        int row = 0, limit = std::max(1, std::min(5, (int)((h - 360) / 42)));
        for (auto it = g_World.affinity.events.rbegin(); it != g_World.affinity.events.rend() && row < limit; ++it)
        {
            if (it->npcId != n.id) continue;
            float ry = y + 262 + row++ * 42.f;
            Text(dx, ry, std::wstring(Affinity::ReasonName(it->reason)) + L"  " + Affinity::Signed(it->after - it->before),
                 AffinityColor(it->after - it->before), 14);
            Text(dx, ry + 20, std::to_wstring((int)std::max(0.0, g_World.society.elapsed - it->time)) + L"\ucd08 \uc804  \u00b7  " +
                 Affinity::Signed(it->before) + L" \u2192 " + Affinity::Signed(it->after), g_Muted, 11);
        }
        if (!row) Text(dx, y + 264, L"\uae30\ub85d\ub41c \ubcc0\ud654\uac00 \uc5c6\uc2b5\ub2c8\ub2e4.", g_Muted, 14);
    }
    Wrap(dx, y + h - 82, 298, L"\ub300\ud654 +5 \u00b7 \ube75 +25 \u00b7 \uce58\ub8cc +35\n\uac70\ub798 +2 \u00b7 \uc218\ub828 +5\n\uc704\ud611 -40 \u00b7 \uacf5\uaca9 -15", g_Muted, 12);
}

void RenderUI()
{
    if (g_Panel == 4)
    {
        AvatarPanel();
        return;
    }
    g_UiScale = std::min(1.f, std::min(g_Width / 960.f, g_Height / 640.f));
    g_Buttons.clear();
    auto& p = g_World.player;
    float w = UiWidth(), h = UiHeight();
    if (!g_Dialog && !g_Panel)
    {
        Text(24, 110, Story::Title(g_World.story.stage), g_Accent, 18);
        Wrap(24, 139, 300, Story::Objective(g_World), g_Muted, 14);
        AddButton(24, 222, 140, 40, L"\uc5ec\uc815", StoryOpen);
        AddButton(172, 222, 140, 40, L"\uacbd\uc791\uc9c0 \uc785\uc7a5", EnterLevel);
        int village = Society::NearestVillage(p.p);
        const auto& market = g_World.society.villages[village];
        Text(
            24,
            280,
            std::wstring(Society::VillageName(village)) + L"  /  " +
                std::to_wstring((int)Distance(p.p, VillageCenter(village))) + L"m",
            g_Ink,
            15
        );
        Text(
            24,
            305,
            L"\uc2dc\uc7a5 \ubb3c\ud488 " + std::to_wstring(market.stock) + L"  \uacf5\ub3d9 \uae08\uace0 " +
                std::to_wstring(market.treasury),
            g_Muted,
            13
        );
        int row = 0;
        for (auto it = g_World.society.events.rbegin(); it != g_World.society.events.rend() && row < 3; ++it, ++row)
        {
            Text(24, 336 + row * 22.f, Society::EventText(*it), g_Muted, 13);
        }
    }
    Box(0, 0, w, 88, Color(.055f, .08f, .075f, .93f));
    Line(0, 88, w, 88, 1, Color(.34f, .39f, .33f, .7f));
    Text(24, 14, p.profile.name, g_Ink, 16);
    Text(24, 46, L"\uc0dd\uba85", g_Muted, 13);
    Bar(66, 55, 162, p.health, Color(.63f, .31f, .28f));
    Text(237, 45, std::to_wstring((int)p.health), g_Ink, 14);
    Text(24, 66, L"\uae30\ub825", g_Muted, 12);
    Bar(66, 74, 162, p.stamina, Color(.36f, .57f, .49f));
    Text(300, 46, L"\uc740\ud654 " + std::to_wstring(g_World.society.gold), g_Accent, 16);
    if (w > 1120)
    {
        Text(w * .5f - 70, 18, L"AGE OF ECHOES", g_Ink, 17);
        Text(w * .5f - 44, 49, L"\uacc4\uc2b9\uc758 \ubc95\uce59", g_Muted, 14);
    }
    size_t alive = (size_t)std::count_if(
        g_World.npcs.begin(),
        g_World.npcs.end(),
        [](const Npc& n)
        {
            return n.life.alive;
        }
    );
    Text(w - 215, 19, L"\uc0b4\uc544\uc788\ub294 \uc8fc\ubbfc  " + std::to_wstring(alive), g_Ink, 18);
    Text(w - 215, 49, L"\ub098\uc758 \uc8fd\uc74c  " + std::to_wstring(g_World.deaths), g_Muted, 14);
    if (!g_Panel && !g_Dialog)
    {
        Map();
    }
    Box(0, h - 76, w, 76, Color(.055f, .08f, .075f, .95f));
    Line(0, h - 76, w, h - 76, 1, Color(.34f, .39f, .33f));
    Text(
        24,
        h - 59,
        L"\ube75  " + std::to_wstring(p.bread) + L"     \uc57d\ucd08  " + std::to_wstring(p.herbs) +
            L"     \ubaa9\uc7ac  " + std::to_wstring(p.wood) + L"     \uae30\uc5b5 \ud30c\ud3b8  " +
            std::to_wstring(p.fragments),
        g_Ink,
        15
    );
    Text(
        24,
        h - 30,
        std::wstring(L"\uace0\uc720\ub2a5\ub825 \u00b7 ") + AbilityName(p.ability) + L" / \uc9d5\uc870 \uc77d\uae30",
        g_Muted,
        14
    );
    IconButton(w - 160, h - 58, Journal, L"\uae30\ub85d");
    IconButton(w - 112, h - 58, Abilities, L"\ub2a5\ub825");
    IconButton(w - 64, h - 58, SaveNow, L"\uc800\uc7a5");
    if (!g_Dialog && !g_Panel)
    {
        AddButton(300, 14, 112, 28, L"\uafb8\ubbf8\uae30", AvatarOpen);
        AddButton(424, 14, 116, 28, L"\ud638\uac10\ub3c4  H", AffinityOpen);
    }
    AddButton(w - 446, h - 58, 124, 40, L"\uc9d5\uc870 \uc77d\uae30", CastInnate, p.stamina >= 25);
    AddButton(
        w - 312,
        h - 58,
        134,
        40,
        g_Secondary == Matter ? L"\uc791\uc740 \ubd88\uc528" : L"\uace0\uc694\ud55c \ud638\ud761",
        CastAdjacent,
        p.training[g_Secondary] > 0 && p.stamina >= 25
    );
    if (g_TargetKind >= 0 && !g_Dialog && !g_Panel)
    {
        float cw = 500, cx = (w - cw) * .5f;
        std::wstring name = TargetName();
        if (name.size() > 22)
        {
            name = name.substr(0, 21) + L"...";
        }
        Box(cx, h - 153, cw, 60, Color(.065f, .09f, .08f, .96f));
        Text(cx + 16, h - 136, name, g_Ink, 15);
        AddButton(cx + cw - 110, h - 143, 100, 40, L"\uc0b4\ud3b4\ubcf4\uae30", Open);
    }
    if (g_Panel)
    {
        if (g_Panel == 5)
        {
            AffinityPanel();
        }
        else if (g_Panel == 3)
        {
            StoryPanel();
        }
        else
        {
            SidePanel();
        }
    }
    if (g_Dialog)
    {
        g_Buttons.clear();
        Box(0, 0, w, h, Color(.01f, .025f, .02f, .64f));
        auto choices = Choices();
        float dw = 640, dh = std::max(372.f, 184.f + choices.size() * 48.f);
        float x = (w - dw) * .5f, y = (h - dh) * .5f;
        Box(x, y, dw, dh, Color(.08f, .105f, .10f, .99f));
        Line(x, y, x + dw, y, 2, g_Accent);
        Text(x + 26, y + 22, g_DeathConfirm ? L"\uacc4\uc2b9\uc758 \ubc95\uce59" : TargetName(), g_Ink, 22);
        Wrap(x + 26, y + 67, dw - 52, Description(), g_Muted, 16);
        if (g_TargetKind == 2 && !g_DeathConfirm)
        {
            const auto& n = g_World.npcs[g_TargetIndex];
            Text(x + 26, y + 144, L"\ud638\uac10\ub3c4 " + Affinity::Signed(n.relationship) + L"  " + Affinity::Tier(n.relationship) +
                 L"  \u00b7  \uc2e0\ub8b0 " + std::to_wstring(n.trust) + L"/3", AffinityColor(n.relationship), 14);
            if (n.affinity.lastChange >= 0)
                Text(x + 300, y + 144, std::wstring(Affinity::ReasonName(n.affinity.lastReason)) + L" " +
                     Affinity::Signed(n.affinity.lastDelta), g_Accent, 13);
        }
        float by = y + 178;
        for (const auto& c : choices)
        {
            AddButton(x + 26, by, dw - 52, 40, c.text, c.action, c.enabled);
            by += 48;
        }
        g_Buttons.push_back({x + dw - 48, y + 14, 34, 34, Close, true, L"\ub2eb\uae30"});
        Line(x + dw - 37, y + 25, x + dw - 25, y + 37, 1.5f, g_Muted);
        Line(x + dw - 25, y + 25, x + dw - 37, y + 37, 1.5f, g_Muted);
        if (choices.empty())
        {
            Text(
                x + 26,
                by,
                L"\uc774\uacf3\uc5d0\ub294 \ub2f9\uc2e0\uc774 \ub0a8\uae34 \ud754\uc801\uc774 \uc788\uc2b5\ub2c8\ub2e4.",
                g_Muted,
                15
            );
        }
    }
    if (g_ToastTime > 0 && !g_Dialog && !g_Panel)
    {
        float tw = std::min(720.f, w - 48);
        Box((w - tw) * .5f, h - 225, tw, 60, Color(.07f, .105f, .09f, .98f));
        Wrap((w - tw) * .5f + 18, h - 214, tw - 36, g_Toast, g_Ink, 15);
    }
    g_Renderer->Flush();
}

void Display()
{
    if (!g_Renderer)
    {
        return;
    }
    g_Renderer->SetGameFrameTiming(g_UpdateMs, g_PreviousSwapMs);
    g_Renderer->BeginFrame();
    g_Renderer->BeginWorld();
    if (InCombat())
    {
        g_LevelView->DrawWorld();
    }
    else
    {
        g_Scene->cameraOverride = false;
        g_Scene->Render(g_TargetKind, g_TargetIndex, g_Sites);
    }
    g_Renderer->EndWorld();
    g_Scene->DrawOverlay(
        []
        {
            if (g_Panel == 4)
            {
                AvatarPanel();
            }
            else if (InCombat())
            {
                g_UiScale = std::min(1.f, std::min(g_Width / 960.f, g_Height / 640.f));
                g_Buttons.clear();
                g_LevelView->DrawHud(g_MouseX, g_MouseY);
                AddButton(UiWidth() - 186, 286, 160, 32, L"\uafb8\ubbf8\uae30", AvatarOpen);
            }
            else
            {
                RenderUI();
            }
        }
    );
    g_Renderer->EndFrame();
    auto swapStart = std::chrono::steady_clock::now();
    glfwSwapBuffers(g_Window);
    g_PreviousSwapMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - swapStart).count();
}

IsometricView::Point MovementInput()
{
    return IsometricView::GroundFromScreen({(double)g_Keys['d'] - g_Keys['a'], (double)g_Keys['s'] - g_Keys['w']});
}

void Update(float dt)
{
    if (g_Panel == 4)
    {
        g_Scene->playerMotion.Stop();
        return;
    }
    if (InCombat())
    {
        if (!g_Focused || g_LevelView->paused || CurrentLevel().judgementPending)
        {
            return;
        }
        auto& level = CurrentLevel();
        g_Scene->UpdateSociety(dt, false);
        int phase = level.phase;
        int kills = level.kills;
        int souls = level.souls;
        auto previous = level.p;
        auto input = MovementInput();
        LevelOne::Point movement(input.x, input.y);
        g_LevelView->Update(dt, movement, g_Keys[' ']);
        g_Scene->playerMotion.Update(
            movement.x,
            movement.y,
            level.p.x - previous.x,
            level.p.y - previous.y,
            g_Keys[' '] && level.stamina > 2
        );
        g_World.player.echo[0] += (level.kills - kills) * 3;
        g_World.player.echo[3] += level.souls - souls;
        if (phase == LevelOne::Fighting)
        {
            g_World.time += dt;
            g_World.dirty = true;
            if (level.phase == LevelOne::Defeated)
            {
                int cause = level.encounter == 1   ? Story::VillageRaid
                            : level.encounter == 2 ? Story::ChurchBattle
                            : level.encounter == 3 ? Story::FinalBattle
                                                   : Story::Field;
                Die(g_World, cause);
                Persist();
            }
            else if (level.phase == LevelOne::Cleared)
            {
                if (level.encounter)
                {
                    Story::FinishBattle(g_World);
                }
                else
                {
                    g_World.player.fragments += 3;
                    g_World.player.echo[6] += 12;
                    g_World.Record(FieldCleared);
                }
                Persist();
            }
        }
        return;
    }
    if (!g_Focused)
    {
        return;
    }
    g_World.time += dt;
    int previousDeaths = g_World.deaths;
    g_Scene->UpdateSociety(dt, true);
    Story::UpdateWorld(g_World);
    if (g_World.deaths != previousDeaths)
    {
        g_Dialog = g_DeathConfirm = false;
        g_Panel = 0;
        Notify(Story::DeathMessage(g_World));
    }
    if (g_Dialog && g_TargetKind == 2 &&
        (!g_World.npcs[g_TargetIndex].life.alive || Distance(g_World.npcs[g_TargetIndex].p, g_World.player.p) > 90))
    {
        g_Dialog = false;
    }
    if (g_Dialog || g_Panel)
    {
        g_Scene->playerMotion.Stop();
        return;
    }
    auto movement = MovementInput();
    g_Scene->Update(dt, Vec(movement.x, movement.y), g_Keys[' ']);
    RefreshTargets();
}

void Idle()
{
    auto updateStart = std::chrono::steady_clock::now();
    double now = glfwGetTime();
    float dt = (float)std::max(0.0, std::min(.05, now - g_LastTick));
    g_LastTick = now;
    g_ToastTime = std::max(0.f, g_ToastTime - dt);
    Update(dt);
    if (g_Monitor)
    {
        bool paused =
            !g_Focused || g_Panel == 4 || (InCombat() && (g_LevelView->paused || CurrentLevel().judgementPending));
        g_Monitor->Publish(g_World, paused);
    }
    g_Autosave += dt;
    if (g_Autosave >= 15)
    {
        g_Autosave = 0;
        if (!g_SaveBlocked)
        {
            Persist();
        }
    }
    g_UpdateMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - updateStart).count();
}

void Reshape(int w, int h)
{
    g_Width = std::max(1, w);
    g_Height = std::max(1, h);
    if (g_Renderer)
    {
        g_Renderer->Resize(g_Width, g_Height);
    }
    if (g_Scene)
    {
        g_Scene->width = g_Width;
        g_Scene->height = g_Height;
    }
}

void KeyDown(unsigned char key, int, int)
{
    if (key >= 'A' && key <= 'Z')
    {
        key += 32;
    }
    if (g_Keys[key])
    {
        return;
    }
    g_Keys[key] = true;
    if (InCombat())
    {
        if (key == 27 || key == 'p')
        {
            LevelCommand(LevelOneView::TogglePause);
        }
        return;
    }
    if (key == 27)
    {
        Act(Close);
        return;
    }
    if (g_Dialog)
    {
        if (key >= '1' && key <= '9')
        {
            auto c = Choices();
            size_t i = key - '1';
            if (i < c.size())
            {
                Act(c[i].action);
            }
        }
        return;
    }
    if (key == 'e')
    {
        Act(Open);
    }
    if (key == 'f' && !g_Panel)
    {
        int target = 0;
        double nearest = 65;
        for (const auto& n : g_World.npcs)
        {
            double distance = Distance(n.p, g_World.player.p);
            if (n.life.alive && distance < nearest && (n.id != g_World.story.childId || g_World.story.childComplete))
            {
                nearest = distance;
                target = n.id;
            }
        }
        Society::PlayerAttack(g_World, target);
    }
    if (key == 'j')
    {
        Act(Journal);
    }
    if (key == 'h')
    {
        Act(AffinityOpen);
    }
    if (key == 'm')
    {
        Act(StoryOpen);
    }
    if (key == 'c')
    {
        Act(Abilities);
    }
    if (key == 'q')
    {
        AbilityUse(false);
    }
    if (key == 'r')
    {
        AbilityUse(true);
    }
}

void KeyUp(unsigned char key, int, int)
{
    if (key >= 'A' && key <= 'Z')
    {
        key += 32;
    }
    g_Keys[key] = false;
}

void SpecialKey(int key, int, int)
{
    if (key == GLFW_KEY_F8)
    {
        bool loaded = Art::Reload();
        std::cout << "[Art] reload=" << (loaded ? "ok" : "failed; previous catalog retained") << '\n';
        return;
    }
    if (key == GLFW_KEY_F1 && !InCombat())
    {
        Act(EnterLevel);
        return;
    }
    if (key == GLFW_KEY_F6 && g_Renderer)
    {
        auto settings = g_Renderer->GetPostProcessing();
        settings.enabled = !settings.enabled;
        g_Renderer->SetPostProcessing(settings);
        std::cout << "Post-processing: " << (settings.enabled ? "on" : "off") << '\n';
    }
}

void Mouse(int button, int state, int x, int y)
{
    g_MouseX = x;
    g_MouseY = y;
    if (button != GLFW_MOUSE_BUTTON_LEFT || state != GLFW_PRESS)
    {
        return;
    }
    if (g_Panel == 4 || InCombat())
    {
        for (auto i = g_Buttons.rbegin(); i != g_Buttons.rend(); ++i)
        {
            if (i->enabled && Hover(i->x, i->y, i->w, i->h))
            {
                Act(i->action);
                return;
            }
        }
        if (g_Panel == 4)
        {
            g_ProfileField = 0;
            return;
        }
        LevelCommand(g_LevelView->Hit(x, y));
        return;
    }
    for (auto i = g_Buttons.rbegin(); i != g_Buttons.rend(); ++i)
    {
        if (i->enabled && Hover(i->x, i->y, i->w, i->h))
        {
            int action = i->action;
            Act(action);
            return;
        }
    }
}

void Motion(int x, int y)
{
    g_MouseX = x;
    g_MouseY = y;
}

void Wheel(int, int direction, int, int)
{
    if (g_Panel == 4)
    {
        return;
    }
    if (InCombat())
    {
        if (!g_LevelView->paused && CurrentLevel().phase == LevelOne::Fighting)
        {
            g_Scene->zoom = std::max(.8f, std::min(2.f, g_Scene->zoom + direction * .1f));
        }
        return;
    }
    if (g_Panel == 5)
    {
        Act(direction > 0 ? AffinityPrevious : AffinityNext);
    }
    else if (g_Panel == 1)
    {
        g_JournalOffset =
            std::max(0, std::min(std::max(0, (int)g_World.events.size() - 1), g_JournalOffset - direction * 2));
    }
    else if (!g_Dialog && !g_Panel)
    {
        g_Scene->zoom = std::max(.8f, std::min(2.f, g_Scene->zoom + direction * .1f));
    }
}

void Entry(int state)
{
    if (state == GLFW_FALSE)
    {
        std::fill(std::begin(g_Keys), std::end(g_Keys), false);
    }
}

void Visibility(int state)
{
    g_Focused = state == GLFW_TRUE;
    if (!g_Focused)
    {
        std::fill(std::begin(g_Keys), std::end(g_Keys), false);
    }
}

void OnClose()
{
    if (g_Monitor)
    {
        g_Monitor->Publish(g_World, true, true);
        g_Monitor.reset();
    }
    if (!g_SaveBlocked)
    {
        Persist();
    }
    g_LevelView.reset();
    g_Scene.reset();
    g_Renderer.reset();
}

int main(int argc, char** argv)
{
    glfwSetErrorCallback(
        [](int code, const char* description)
        {
            std::cerr << "[GLFW] error=" << code << " " << description << '\n';
        }
    );
    if (!glfwInit())
    {
        return 1;
    }

    struct WindowLifetime
    {
        ~WindowLifetime()
        {
            if (g_Window)
            {
                glfwDestroyWindow(g_Window);
            }
            g_Window = nullptr;
            glfwTerminate();
        }
    } lifetime;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    if (auto* monitor = glfwGetPrimaryMonitor())
    {
        if (const auto* mode = glfwGetVideoMode(monitor))
        {
            g_Width = std::max(800, std::min(g_Width, mode->width - 100));
            g_Height = std::max(600, std::min(g_Height, mode->height - 140));
        }
    }
    g_Window = glfwCreateWindow(g_Width, g_Height, "Age of Echoes", nullptr, nullptr);
    if (!g_Window)
    {
        return 1;
    }
    glfwSetWindowPos(g_Window, 60, 40);
    glfwMakeContextCurrent(g_Window);
    int swapInterval = 1;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--no-vsync")
        {
            swapInterval = 0;
        }
    }
    glfwSwapInterval(swapInterval);
    std::cout << "[Window] backend=GLFW version=" << glfwGetVersionString() << " swapInterval=" << swapInterval
              << " samples=4\n";
    glfwGetFramebufferSize(g_Window, &g_Width, &g_Height);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK || !GLEW_VERSION_3_3)
    {
        std::cerr << "OpenGL 3.3 is required.\n";
        return 1;
    }
    glEnable(GL_MULTISAMPLE);
    g_Renderer.reset(new Renderer(g_Width, g_Height));
    if (!g_Renderer->IsInitialized())
    {
        g_Renderer.reset();
        return 1;
    }
    g_Scene.reset(new GameScene(*g_Renderer, g_World));
    g_Scene->width = g_Width;
    g_Scene->height = g_Height;
    bool loaded = Load(g_World, g_SaveBlocked);
    Society::Initialize(g_World);
    g_Monitor.reset(new WorldMonitor());
    if (!Walkable(g_World.player.p))
    {
        g_World.player.p = VillageCenter(Society::NearestVillage(g_World.player.p));
        g_World.dirty = true;
    }
    if (!Walkable(g_World.player.checkpoint))
    {
        g_World.player.checkpoint = VillageCenter(Society::NearestVillage(g_World.player.checkpoint));
        g_World.dirty = true;
    }
    Story::PrepareMemories(g_World);
    if (!g_World.levelOne.seed)
    {
        LevelOne::Start(g_World.levelOne, NewLevelSeed(), false);
    }
    if (!loaded)
    {
        g_World.levelOne.active = false;
    }
    g_LevelView.reset(new LevelOneView(*g_Scene, CurrentLevel()));
    if (!loaded)
    {
        g_World.Record(Awakened);
    }
    if (!g_World.player.training[Matter] && g_World.player.training[Mind])
    {
        g_Secondary = Mind;
    }
    if (g_SaveBlocked)
    {
        Notify(
            L"\uae30\uc874 \uc800\uc7a5\uc744 \uc77d\uc9c0 \ubabb\ud588\uc2b5\ub2c8\ub2e4. \uc6d0\ubcf8\uc740 \ubcf4\uc874\ub429\ub2c8\ub2e4."
        );
    }
    else
    {
        Notify(
            loaded
                ? L"\ub2f9\uc2e0\uc774 \ub0a8\uae34 \uc138\uacc4\ub85c \ub3cc\uc544\uc654\uc2b5\ub2c8\ub2e4."
                : L"\uaebc\uc9c4 \uc57c\uc601\uc9c0 \ub108\uba38\ub85c \ub204\uad70\uac00\uc758 \uae30\uce68\uc774 \ub4e4\ub9bd\ub2c8\ub2e4."
        );
    }
    RefreshTargets();
    g_LastTick = glfwGetTime();
    if (InCombat() && !g_SaveBlocked)
    {
        LevelOne::Notice(CurrentLevel(), LevelOne::AreaName(CurrentLevel()));
    }
    glfwSetFramebufferSizeCallback(
        g_Window,
        [](GLFWwindow*, int width, int height)
        {
            if (width > 0 && height > 0)
            {
                Reshape(width, height);
            }
        }
    );
    glfwSetKeyCallback(
        g_Window,
        [](GLFWwindow* window, int key, int, int action, int mods)
        {
            if (g_Panel == 4)
            {
                if (action == GLFW_RELEASE)
                {
                    return;
                }
                if (g_ProfileField && (mods & GLFW_MOD_CONTROL))
                {
                    if (key == GLFW_KEY_A)
                    {
                        g_ProfileSelected = true;
                        return;
                    }
                    if (key == GLFW_KEY_V)
                    {
                        const char* clipboard = glfwGetClipboardString(window);
                        wchar_t paste[512] = {};
                        int count = clipboard
                                        ? MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, clipboard, -1, paste, 512)
                                        : 0;
                        for (int i = 0; i < count - 1; ++i)
                        {
                            unsigned code = paste[i];
                            if (code >= 0xd800 && code <= 0xdbff && i + 1 < count - 1 && paste[i + 1] >= 0xdc00 &&
                                paste[i + 1] <= 0xdfff)
                            {
                                code = 0x10000 + ((code - 0xd800) << 10) + (paste[++i] - 0xdc00);
                            }
                            InputProfileCharacter(code);
                        }
                        return;
                    }
                }
                if (key == GLFW_KEY_ESCAPE)
                {
                    Act(AvatarCancel);
                    return;
                }
                if (key == GLFW_KEY_TAB)
                {
                    Act(g_ProfileField == 1 ? AvatarMotto : AvatarName);
                    return;
                }
                if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
                {
                    g_ProfileField = 0;
                    return;
                }
                if (key == GLFW_KEY_BACKSPACE && g_ProfileField)
                {
                    auto& text = g_ProfileField == 1 ? g_ProfileDraft.name : g_ProfileDraft.motto;
                    if (g_ProfileSelected)
                    {
                        text.clear();
                    }
                    else
                    {
                        EraseProfileCharacter(text);
                    }
                    g_ProfileSelected = false;
                }
                return;
            }
            if (action == GLFW_REPEAT)
            {
                return;
            }
            if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25)
            {
                if (action == GLFW_PRESS)
                {
                    SpecialKey(key, 0, 0);
                }
                return;
            }
            int mapped = -1;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
            {
                mapped = 'a' + key - GLFW_KEY_A;
            }
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
            {
                mapped = '0' + key - GLFW_KEY_0;
            }
            else if (key == GLFW_KEY_ESCAPE)
            {
                mapped = 27;
            }
            else if (key == GLFW_KEY_SPACE)
            {
                mapped = ' ';
            }
            if (mapped < 0)
            {
                return;
            }
            if (action == GLFW_PRESS)
            {
                KeyDown((unsigned char)mapped, 0, 0);
            }
            else if (action == GLFW_RELEASE)
            {
                KeyUp((unsigned char)mapped, 0, 0);
            }
        }
    );
    glfwSetCharCallback(
        g_Window,
        [](GLFWwindow*, unsigned int code)
        {
            InputProfileCharacter(code);
        }
    );
    glfwSetCursorPosCallback(
        g_Window,
        [](GLFWwindow* window, double x, double y)
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            Motion((int)(x * g_Width / std::max(1, width)), (int)(y * g_Height / std::max(1, height)));
        }
    );
    glfwSetMouseButtonCallback(
        g_Window,
        [](GLFWwindow* window, int button, int action, int)
        {
            double x, y;
            int width, height;
            glfwGetCursorPos(window, &x, &y);
            glfwGetWindowSize(window, &width, &height);
            Mouse(button, action, (int)(x * g_Width / std::max(1, width)), (int)(y * g_Height / std::max(1, height)));
        }
    );
    glfwSetScrollCallback(
        g_Window,
        [](GLFWwindow*, double, double y)
        {
            if (y != 0)
            {
                Wheel(0, y > 0 ? 1 : -1, g_MouseX, g_MouseY);
            }
        }
    );
    glfwSetCursorEnterCallback(
        g_Window,
        [](GLFWwindow*, int entered)
        {
            Entry(entered);
        }
    );
    glfwSetWindowFocusCallback(
        g_Window,
        [](GLFWwindow* window, int focused)
        {
            Visibility(focused && !glfwGetWindowAttrib(window, GLFW_ICONIFIED));
        }
    );
    glfwSetWindowIconifyCallback(
        g_Window,
        [](GLFWwindow* window, int iconified)
        {
            Visibility(!iconified && glfwGetWindowAttrib(window, GLFW_FOCUSED));
        }
    );
    Visibility(glfwGetWindowAttrib(g_Window, GLFW_FOCUSED));
    std::cout << "Level 1: auto-fire | WASD move | Space sprint | Esc/P pause | F1 resume field from world\n"
              << "F6 toggle post-processing\n"
              << "WASD move | Space sprint | E interact | 1-3 choose | Q innate | R adjacent\n"
              << "J journal | C abilities | wheel zoom | Esc close panel | window close saves\n";
    while (!glfwWindowShouldClose(g_Window))
    {
        int width, height;
        glfwGetFramebufferSize(g_Window, &width, &height);
        if (glfwGetWindowAttrib(g_Window, GLFW_ICONIFIED) || width == 0 || height == 0)
        {
            glfwWaitEventsTimeout(.1);
        }
        else
        {
            glfwPollEvents();
        }
        if (glfwWindowShouldClose(g_Window))
        {
            break;
        }
        Idle();
        if (width > 0 && height > 0 && !glfwGetWindowAttrib(g_Window, GLFW_ICONIFIED))
        {
            Display();
        }
    }
    // Renderer resources must be released while their GL context is still current.
    OnClose();
    return 0;
}
