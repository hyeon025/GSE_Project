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
#include "Dependencies/glew.h"
#include "Dependencies/freeglut.h"
#include "Renderer.h"
#include "GameWorld.h"
#include "GameScene.h"
#include "LevelOneView.h"

using namespace Game;
World g_World;
std::unique_ptr<Renderer> g_Renderer;
std::unique_ptr<GameScene> g_Scene;
std::unique_ptr<LevelOneView> g_LevelView;
std::vector<Site> g_Sites;
bool g_Keys[256] = {};
int g_LastTick = 0, g_Width = 1440, g_Height = 900;
int g_TargetKind = -1, g_TargetIndex = -1, g_Panel = 0;
int g_JournalOffset = 0;
Ability g_Secondary = Matter;
int g_MouseX = 0, g_MouseY = 0;
bool g_Dialog = false, g_DeathConfirm = false, g_SaveBlocked = false, g_Focused = true;
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
    EnterLevel = 60
};

void AbilityUse(bool secondary);

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
    if (g_World.levelOne.active)
    {
        LevelOne::Notice(g_World.levelOne, s);
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
    auto& level = g_World.levelOne;
    switch (command)
    {
        case LevelOneView::TogglePause:
            if (level.phase == LevelOne::Fighting)
            {
                g_LevelView->paused = !g_LevelView->paused;
            }
            break;
        case LevelOneView::Resume:
            g_LevelView->paused = false;
            break;
        case LevelOneView::Retry:
            if (level.phase == LevelOne::Fighting)
            {
                return;
            }
            LevelOne::Start(level, NewLevelSeed());
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
        return std::wstring(Personality(g_World.npcs[g_TargetIndex].archetype)) + L" \u00b7 " +
               std::to_wstring(g_World.npcs[g_TargetIndex].id) + L"\ubc88\uc9f8 \uacc4\uc2b9\uc790";
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
        c.push_back({L"\uc0b4\uc544\uc628 \uc774\uc57c\uae30\ub97c \ub4e3\ub294\ub2e4", Talk, true});
        c.push_back({L"\ube75\uc744 \ub098\ub208\ub2e4  \u00b7  \ube75 1", Share, p.bread > 0 && n.trust < 3});
        bool related = Adjacent(p.ability, n.ability);
        c.push_back(
            {related ? std::wstring(AbilityName(n.ability)) +
                           L" \uacc4\uc5f4 \uc218\ub828  \u00b7  \uc2e0\ub8b0 1, \uae30\ub825 25"
                     : L"\uc774 \uacc4\uc5f4\uc740 \ub0b4 \uace0\uc720\ub2a5\ub825\uacfc \uba40\ub2e4",
             TrainNpc,
             related && n.trust > 0 && !p.training[n.ability] && p.stamina >= 25}
        );
        return c;
    }
    if (g_TargetKind != 1)
    {
        return c;
    }
    const auto& s = g_Sites[g_TargetIndex];
    auto st = State(g_World, s.key);
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
        return L"\ud558\ub098\uc758 \uc8fd\uc74c\uc740 \ud558\ub098\uc758 \uc0dd\uba85\uc744 \ub0a8\uae34\ub2e4. \uc9c0\uae08\uae4c\uc9c0\uc758 \uc120\ud0dd\uc740 \uc0c8 \uc778\ubb3c\uc758 \uc131\ud5a5\uc73c\ub85c \uc774\uc5b4\uc9c0\uace0, \ub098\ub294 \ub9c8\uc9c0\ub9c9\uc73c\ub85c \uc26c\uc5c8\ub358 \uacf3\uc5d0\uc11c \ub2e4\uc2dc \ub208\uc744 \ub72c\ub2e4.";
    }
    if (g_TargetKind == 2)
    {
        auto& n = g_World.npcs[g_TargetIndex];
        return std::wstring(
                   L"\uadf8\uc5d0\uac8c\uc11c \ub0af\uc775\uc740 \uc0b6\uc758 \ubc29\ud5a5\uc774 \ub290\uaef4\uc9c4\ub2e4.\n\ud0c0\uace0\ub09c \uacc4\uc5f4: "
               ) +
               AbilityName(n.ability) + L"   \u00b7   " +
               (n.trust > 0 ? L"\ub2f9\uc2e0\uc744 \uc2e0\ub8b0\ud55c\ub2e4."
                            : L"\uc544\uc9c1 \ub2f9\uc2e0\uc744 \ubaa8\ub978\ub2e4.");
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
    if (action == EnterLevel && !g_World.levelOne.active)
    {
        g_World.levelOne.active = true;
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
        Notify(
            L"\ud558\ub098\uc758 \uc0b6\uc774 \ub05d\ub098\uace0, \uc0c8\ub85c\uc6b4 \uacc4\uc2b9\uc790\uac00 \ud0dc\uc5b4\ub0ac\uc2b5\ub2c8\ub2e4."
        );
        RefreshTargets();
        Persist();
        return;
    }
    if (g_TargetKind == 2)
    {
        auto& n = g_World.npcs[g_TargetIndex];
        if (action == Talk)
        {
            const wchar_t* stories[] = {
                L"\ubb34\uae30\ub97c \ub193\uc73c\uba74 \ub610 \ub204\uad70\uac00 \ub2e4\uce60 \uac83 \uac19\uc18c.",
                L"\uc774\uc720\ub294 \ubaa8\ub974\uaca0\uc9c0\ub9cc, \uad76\uc8fc\ub9b0 \uc774\ub97c \uc9c0\ub098\uce60 \uc218 \uc5c6\uc5b4\uc694.",
                L"\uc790\uafb8 \ub4a4\ub97c \ub3cc\uc544\ubcf4\uac8c \ub3fc\uc694. \ubb34\uc5c7\uc744 \ud53c\ud558\ub294\uc9c0\ub3c4 \ubaa8\ub974\uba74\uc11c.",
                L"\uc800 \ud3d0\ud5c8 \ub108\uba38\uc5d0 \ubb34\uc5c7\uc774 \uc788\ub294\uc9c0 \uc54c\uace0 \uc2f6\uc5b4\uc694.",
                L"\ub0b4\uac00 \ub300\uc2e0 \uc544\ud50c \uc218 \uc788\ub2e4\uba74, \uadf8\uac78\ub85c \ub410\uc5b4\uc694.",
                L"\uc870\uae08\ub9cc \ub354 \ubaa8\uc73c\uba74 \uc548\uc2ec\ud560 \uc218 \uc788\uc744 \uac83 \uac19\uc18c.",
                L"\uc5b4\ub518\uac00\uc5d0\ub294 \uc544\uc9c1 \ubd88\uc774 \ucf1c\uc9c4 \uc9d1\uc774 \uc788\uc744 \uac70\uc608\uc694."
            };
            Notify(stories[n.archetype]);
            g_World.Record(Remembered);
        }
        if (action == Share)
        {
            --p.bread;
            ++n.trust;
            p.echo[1] += 5;
            p.echo[6] += 2;
            g_World.Record(Befriended);
            Notify(L"\uadf8\uac00 \ub2f9\uc2e0 \uacc1\uc5d0 \uc870\uae08 \ub354 \uac00\uae4c\uc774 \uc120\ub2e4.");
        }
        if (action == TrainNpc)
        {
            p.training[n.ability] = 1;
            g_Secondary = n.ability;
            p.stamina -= 25;
            g_World.Record(Learned);
            Notify(L"\uc778\uc811 \uacc4\uc5f4\uc758 \uc57d\ud55c \uae30\uc220\uc744 \uc775\ud614\uc2b5\ub2c8\ub2e4.");
        }
    }
    else
    {
        const auto s = g_Sites[g_TargetIndex];
        auto& st = g_World.states[s.key];
        switch (action)
        {
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
        Die(g_World);
        Notify(
            L"\uae30\uc5b5\uc758 \ub300\uac00\ub85c \uc0b6\uc744 \uc783\uc5c8\uc2b5\ub2c8\ub2e4. \uc0c8\ub85c\uc6b4 \uc0dd\uba85\uc774 \ub0a8\uc558\uc2b5\ub2c8\ub2e4."
        );
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
            std::wstring direction = std::abs(q.x - g_Width * .5) > std::abs(q.y - g_Height * .55)
                                         ? (q.x > g_Width * .5 ? L"\ub3d9\ucabd" : L"\uc11c\ucabd")
                                         : (q.y > g_Height * .55 ? L"\ub0a8\ucabd" : L"\ubd81\ucabd");
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
            Vec p(g_World.player.p.x + sx * .5 - sy, g_World.player.p.y - sx * .5 - sy);
            Box(x + ix * 4, y + iy * 4, 4, 4, Road(p) < 33 ? Color(.39f, .40f, .33f) : Color(.16f, .21f, .18f));
        }
    }
    auto mark = [&](Vec p, Color c, float radius)
    {
        double dx = p.x - g_World.player.p.x, dy = p.y - g_World.player.p.y;
        float mx = (float)((dx - dy) / 3), my = (float)(-(dx + dy) / 6);
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
        mark(n.p, Color(.68f, .67f, .82f), 2.5f);
    }
    mark(g_World.player.p, g_Ink, 3.5f);
    Text(x + 8, y + 7, L"N", g_Ink, 13);
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

void RenderUI()
{
    g_UiScale = std::min(1.f, std::min(g_Width / 960.f, g_Height / 640.f));
    g_Buttons.clear();
    auto& p = g_World.player;
    float w = UiWidth(), h = UiHeight();
    if (!g_Dialog && !g_Panel)
    {
        AddButton(24, 164, 162, 40, L"\uacbd\uc791\uc9c0 \uc785\uc7a5", EnterLevel);
    }
    Box(0, 0, w, 88, Color(.055f, .08f, .075f, .93f));
    Line(0, 88, w, 88, 1, Color(.34f, .39f, .33f, .7f));
    Text(24, 14, L"\uc7ac\uc758 \ubc29\ub791\uc790", g_Ink, 19);
    Text(24, 46, L"\uc0dd\uba85", g_Muted, 13);
    Bar(66, 55, 162, p.health, Color(.63f, .31f, .28f));
    Text(237, 45, std::to_wstring((int)p.health), g_Ink, 14);
    Text(24, 66, L"\uae30\ub825", g_Muted, 12);
    Bar(66, 74, 162, p.stamina, Color(.36f, .57f, .49f));
    if (w > 1120)
    {
        Text(w * .5f - 90, 18, L"INHERITANCE OF ASH", g_Ink, 17);
        Text(w * .5f - 44, 49, L"\uacc4\uc2b9\uc758 \ubc95\uce59", g_Muted, 14);
    }
    Text(w - 215, 19, L"\uc774\uc5b4\uc9c4 \uc0dd\uba85  " + std::to_wstring(g_World.npcs.size()), g_Ink, 18);
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
        SidePanel();
    }
    if (g_Dialog)
    {
        g_Buttons.clear();
        Box(0, 0, w, h, Color(.01f, .025f, .02f, .64f));
        float dw = 640, dh = 372, x = (w - dw) * .5f, y = (h - dh) * .5f;
        Box(x, y, dw, dh, Color(.08f, .105f, .10f, .99f));
        Line(x, y, x + dw, y, 2, g_Accent);
        Text(x + 26, y + 22, g_DeathConfirm ? L"\uacc4\uc2b9\uc758 \ubc95\uce59" : TargetName(), g_Ink, 22);
        Wrap(x + 26, y + 67, dw - 52, Description(), g_Muted, 16);
        auto choices = Choices();
        float by = y + 178;
        for (const auto& c : choices)
        {
            AddButton(x + 26, by, dw - 52, 44, c.text, c.action, c.enabled);
            by += 52;
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
    if (g_ToastTime > 0 && !g_Dialog)
    {
        float tw = std::min(720.f, w - 48);
        Box((w - tw) * .5f, 98, tw, 60, Color(.07f, .105f, .09f, .98f));
        Wrap((w - tw) * .5f + 18, 109, tw - 36, g_Toast, g_Ink, 15);
    }
    g_Renderer->Flush();
}

void Display()
{
    if (!g_Renderer)
    {
        return;
    }
    g_Renderer->BeginWorld();
    if (g_World.levelOne.active)
    {
        g_LevelView->DrawWorld();
    }
    else
    {
        g_Scene->cameraOverride = false;
        g_Scene->Render(g_TargetKind, g_TargetIndex, g_Sites);
    }
    g_Renderer->EndWorld();
    if (g_World.levelOne.active)
    {
        g_LevelView->DrawHud(g_MouseX, g_MouseY);
    }
    else
    {
        RenderUI();
    }
    glutSwapBuffers();
}

void Update(float dt)
{
    if (g_World.levelOne.active)
    {
        if (!g_Focused || g_LevelView->paused)
        {
            return;
        }
        auto& level = g_World.levelOne;
        int phase = level.phase;
        int kills = level.kills;
        int souls = level.souls;
        auto previous = level.p;
        LevelOne::Point movement;
        if (g_Keys['w'])
        {
            movement.x += 1;
            movement.y += 1;
        }
        if (g_Keys['s'])
        {
            movement.x -= 1;
            movement.y -= 1;
        }
        if (g_Keys['a'])
        {
            movement.x -= 1;
            movement.y += 1;
        }
        if (g_Keys['d'])
        {
            movement.x += 1;
            movement.y -= 1;
        }
        LevelOne::Update(level, dt, movement, g_Keys[' ']);
        double travel = LevelOne::Distance(previous, level.p);
        g_Scene->walk = travel > 0 ? g_Scene->walk + travel * .17 : 0;
        g_World.player.echo[0] += (level.kills - kills) * 3;
        g_World.player.echo[3] += level.souls - souls;
        if (phase == LevelOne::Fighting)
        {
            g_World.time += dt;
            g_World.dirty = true;
            if (level.phase == LevelOne::Defeated)
            {
                Die(g_World);
                Persist();
            }
            else if (level.phase == LevelOne::Cleared)
            {
                g_World.player.fragments += 3;
                g_World.player.echo[6] += 12;
                g_World.Record(FieldCleared);
                Persist();
            }
        }
        return;
    }
    if (!g_Focused || g_Dialog || g_Panel)
    {
        return;
    }
    g_World.time += dt;
    auto& p = g_World.player;
    float x = 0, y = 0;
    if (g_Keys['w'])
    {
        x += 1;
        y += 1;
    }
    if (g_Keys['s'])
    {
        x -= 1;
        y -= 1;
    }
    if (g_Keys['a'])
    {
        x -= 1;
        y += 1;
    }
    if (g_Keys['d'])
    {
        x += 1;
        y -= 1;
    }
    float len = std::sqrt(x * x + y * y);
    if (len > 0)
    {
        bool sprint = g_Keys[' '] && p.stamina > 2;
        float speed = sprint ? 138.f : 85.f;
        Vec before = p.p, next = p.p;
        next.x += x / len * speed * dt;
        if (Walkable(next))
        {
            p.p = next;
        }
        next = p.p;
        next.y += y / len * speed * dt;
        if (Walkable(next))
        {
            p.p = next;
        }
        double travel = Distance(before, p.p);
        p.exploration += travel;
        g_Scene->walk += travel * .17;
        while (p.exploration >= 160)
        {
            p.exploration -= 160;
            ++p.echo[3];
        }
        if (travel > 0 && sprint)
        {
            p.stamina = std::max(0.f, p.stamina - dt * 18);
        }
        if (travel > 0)
        {
            g_World.dirty = true;
        }
    }
    else
    {
        g_Scene->walk = 0;
    }
    p.stamina = std::min(100.f, p.stamina + dt * 6);
    for (auto& n : g_World.npcs)
    {
        if (Distance(n.p, p.p) > 1100)
        {
            continue;
        }
        Vec goal(
            n.home.x + std::sin(g_World.time * .10 + n.id) * 28,
            n.home.y + std::cos(g_World.time * .08 + n.id) * 28
        );
        double d = Distance(n.p, p.p);
        if (n.trust > 0 && (n.archetype == 1 || n.archetype == 4 || n.archetype == 6) && d > 46 && d < 250)
        {
            goal = p.p;
        }
        if (n.archetype == 2 && d < 85)
        {
            goal = Vec(n.p.x + (n.p.x - p.p.x), n.p.y + (n.p.y - p.p.y));
        }
        if (n.archetype == 3)
        {
            goal =
                Vec(n.home.x + std::sin(g_World.time * .07 + n.id) * 90,
                    n.home.y + std::cos(g_World.time * .09 + n.id) * 90);
        }
        double remaining = Distance(n.p, goal);
        if (remaining > 3)
        {
            double step = std::min(remaining, dt * 22.0);
            Vec next(n.p.x + (goal.x - n.p.x) / remaining * step, n.p.y + (goal.y - n.p.y) / remaining * step);
            if (Walkable(next))
            {
                n.p = next;
            }
        }
    }
    RefreshTargets();
}

void Idle()
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = std::min(.05f, (now - g_LastTick) / 1000.f);
    g_LastTick = now;
    g_ToastTime = std::max(0.f, g_ToastTime - dt);
    Update(dt);
    g_Autosave += dt;
    if (g_Autosave >= 15 && !g_Dialog)
    {
        g_Autosave = 0;
        if (!g_SaveBlocked)
        {
            Persist();
        }
    }
    glutPostRedisplay();
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
    if (g_World.levelOne.active)
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
        if (key >= '1' && key <= '3')
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
    if (key == 'j')
    {
        Act(Journal);
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
    if (key == GLUT_KEY_F1 && !g_World.levelOne.active)
    {
        Act(EnterLevel);
        return;
    }
    if (key == GLUT_KEY_F6 && g_Renderer)
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
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN)
    {
        return;
    }
    if (g_World.levelOne.active)
    {
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
    if (g_World.levelOne.active)
    {
        if (!g_LevelView->paused && g_World.levelOne.phase == LevelOne::Fighting)
        {
            g_Scene->zoom = std::max(.8f, std::min(2.f, g_Scene->zoom + direction * .1f));
        }
        return;
    }
    if (g_Panel == 1)
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
    if (state == GLUT_LEFT)
    {
        std::fill(std::begin(g_Keys), std::end(g_Keys), false);
    }
}

void Visibility(int state)
{
    g_Focused = state == GLUT_VISIBLE;
    if (!g_Focused)
    {
        std::fill(std::begin(g_Keys), std::end(g_Keys), false);
    }
}

void OnClose()
{
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
    glutInit(&argc, argv);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_MULTISAMPLE);
    glutSetOption(GLUT_MULTISAMPLE, 4);
    g_Width = std::max(800, std::min(g_Width, glutGet(GLUT_SCREEN_WIDTH) - 100));
    g_Height = std::max(600, std::min(g_Height, glutGet(GLUT_SCREEN_HEIGHT) - 140));
    glutInitWindowSize(g_Width, g_Height);
    glutInitWindowPosition(60, 40);
    glutCreateWindow("Inheritance of Ash");
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
        return 1;
    }
    g_Scene.reset(new GameScene(*g_Renderer, g_World));
    g_Scene->width = g_Width;
    g_Scene->height = g_Height;
    bool loaded = Load(g_World, g_SaveBlocked);
    if (!g_World.levelOne.seed)
    {
        LevelOne::Start(g_World.levelOne, NewLevelSeed(), false);
    }
    g_LevelView.reset(new LevelOneView(*g_Scene, g_World.levelOne));
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
    g_LastTick = glutGet(GLUT_ELAPSED_TIME);
    if (g_World.levelOne.active && !g_SaveBlocked)
    {
        LevelOne::Notice(g_World.levelOne, L"\ub808\ubca8 1 \u00b7 \uc7bf\ube5b \uacbd\uc791\uc9c0");
    }
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutIgnoreKeyRepeat(1);
    glutDisplayFunc(Display);
    glutIdleFunc(Idle);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(KeyDown);
    glutKeyboardUpFunc(KeyUp);
    glutMouseFunc(Mouse);
    glutSpecialFunc(SpecialKey);
    glutPassiveMotionFunc(Motion);
    glutMotionFunc(Motion);
    glutMouseWheelFunc(Wheel);
    glutEntryFunc(Entry);
    glutVisibilityFunc(Visibility);
    glutCloseFunc(OnClose);
    std::cout << "Level 1: auto-fire | WASD move | Space sprint | Esc/P pause | F1 resume field from world\n"
              << "F6 toggle post-processing\n"
              << "WASD move | Space sprint | E interact | 1-3 choose | Q innate | R adjacent\n"
              << "J journal | C abilities | wheel zoom | Esc close panel | window close saves\n";
    glutMainLoop();
    return 0;
}
