#pragma once
#include "GameWorld.h"

namespace Story
{
    inline int Count(const Game::World& w, int condition)
    {
        int count = 0;
        for (const auto& n : w.npcs)
        {
            if (n.origin != 0)
            {
                continue;
            }
            if (condition == 0 || (condition == 1 && n.met) || (condition == 2 && n.saved) ||
                (condition == 3 && n.changed) || (condition == 4 && n.relationship > 70) ||
                (condition == 5 && n.relationship < -70))
            {
                ++count;
            }
        }
        return count;
    }

    inline std::array<int, 7> LifeTraits(const Game::World& w)
    {
        auto traits = w.player.echo;
        auto add = [&traits](const std::array<int, 7>& values)
        {
            for (int i = 0; i < 7; ++i)
            {
                traits[i] = (int)std::min(1000000LL, (long long)traits[i] + values[i]);
            }
        };
        for (const auto& death : w.story.deaths)
        {
            add(death.traits);
        }
        int firstRecorded = w.story.deaths.empty() ? w.deaths + 1 : w.story.deaths.front().id;
        for (const auto& npc : w.npcs)
        {
            if (npc.origin == 0 && npc.originDeathId < firstRecorded)
            {
                add(npc.echo);
            }
        }
        return traits;
    }

    inline bool FreedomReady(const Game::World& w)
    {
        auto traits = LifeTraits(w);
        long long sum = 0;
        for (int value : traits)
        {
            sum += value;
        }
        // A single direction accounting for over 70% of a life is considered extreme.
        bool balanced = sum == 0 || (long long)traits[Dominant(traits)] * 100 <= sum * 70;
        return Count(w, 0) >= 10 && Count(w, 2) >= 5 && Count(w, 3) >= 3 && w.story.childId > 0 &&
               w.story.relics == 63 && w.story.childComplete && balanced;
    }

    inline const wchar_t* Title(int stage)
    {
        static const wchar_t* names[] = {
            L"\uc774\ub984 \uc5c6\ub294 \uc8fd\uc74c",
            L"\uc8fd\uc74c\uc774 \ub0a8\uae34 \uc0ac\ub78c\ub4e4",
            L"\uc131\uacc4\uad50\ub2e8",
            L"\ubd89\uc740 \uc131\uc790 \uc544\ubca8\ub860",
            L"\ud0dc\uc5b4\ub098\uc9c0 \ub9d0\uc558\uc5b4\uc57c \ud560 \uc544\uc774",
            L"\uc5ec\uc12f \ucd5c\ucd08 \ub2a5\ub825\uc790\uc758 \uc720\uc801",
            L"\uacc4\uc2b9\uc758 \ubc95\uce59\uc758 \uc9c4\uc2e4",
            L"\ud50c\ub808\uc774\uc5b4\uac00 \ub9cc\ub4e0 \uc138\uacc4",
            L"\ucd5c\ucd08\uc758 \uacc4\uc2b9\uc790",
            L"\ud0dc\ucd08\uc758 \ubb18\uc9c0",
            L"\uc5d0\ub179",
            L"\uc0b4\uc544 \uc788\ub294 \uc790\uc758 \uc120\ud0dd",
            L"\uadf8 \uc774\ud6c4\uc758 \uc138\uacc4"
        };
        return names[stage];
    }

    inline const wchar_t* QuestId(int stage)
    {
        static const wchar_t* ids[] = {
            L"MAIN_000",
            L"MAIN_100",
            L"MAIN_200",
            L"BOSS_ABELON",
            L"MAIN_300",
            L"MAIN_300",
            L"WORLD_TRUTH",
            L"MAIN_400",
            L"MAIN_500",
            L"AREA_FIRST_GRAVE",
            L"BOSS_ENOCH",
            L"ENDING",
            L"EPILOGUE"
        };
        return ids[stage];
    }

    inline const wchar_t* Faction(int trait)
    {
        static const wchar_t* names[] = {
            L"\ud68c\uc0c9 \uc6a9\ubcd1\ub2e8",
            L"\uad6c\ud638 \uc218\ub3c4\ud68c",
            L"\ubcc0\ubc29 \ud53c\ub09c\ubbfc",
            L"\uc720\uc801 \ud0d0\uc0ac\ub2e8",
            L"\ub9c8\uc744 \uc218\ud638\ub300",
            L"\uac80\uc740 \uc7a5\ubd80 \uc0c1\ub2e8",
            L"\uc7ac\uac74 \uacf5\ub3d9\uccb4"
        };
        return names[trait];
    }

    inline std::wstring Objective(const Game::World& w)
    {
        const auto& s = w.story;
        switch (s.stage)
        {
            case Prologue:
                return L"\ubcd1\uc790, \ubcf4\uae09 \uc218\ub808, \uc57c\uc601\uc9c0\uc5d0\uc11c \uc11c\ub85c \ub2e4\ub978 \uc120\ud0dd\uc744 \ub0a8\uae34\ub2e4. " +
                       std::to_wstring(Bits(s.villageChoices)) + L" / 2";
            case Successors:
                return L"\uc11c\ub85c \ub2e4\ub978 \uacc4\uc2b9\uc790\uc758 \uc774\uc57c\uae30\ub97c \ub4e3\ub294\ub2e4. " +
                       std::to_wstring(Count(w, 1)) + L" / 3";
            case Church:
                return L"\uc57c\uc601\uc9c0\uc758 \ubc30\uae09 \uc7a5\ubd80, \ud589\uc778\uc758 \uc99d\uc5b8, \ud3d0\ud5c8\uc758 \ubc00\uc11c\ub97c \uc870\uc0ac\ud55c\ub2e4. " +
                       std::to_wstring(Bits(s.clues)) + L" / 3";
            case Abelon:
                return L"\ubd89\uc740 \uc608\ubc30\ub2f9\uc5d0\uc11c \uc544\ubca8\ub860\uacfc \ub300\uba74\ud55c\ub2e4.";
            case Child:
                return s.childId
                           ? L"\ubcc0\ubc29 \uc57c\uc601\uc9c0\uc5d0\uc11c " + std::to_wstring(s.childId) +
                                 L"\ubc88\uc9f8 \uacc4\uc2b9\uc790\uc778 \uc544\uc774\ub97c \ub9cc\ub09c\ub2e4."
                           : L"\uc5ec\uc815\uc744 \uc774\uc5b4\uac04\ub2e4. \ub2e4\uc74c \uc8fd\uc74c\uc5d0\uc11c \ud2b9\ubcc4\ud55c \uacc4\uc2b9\uc790\uac00 \ud0dc\uc5b4\ub09c\ub2e4.";
            case Ruins:
                return L"\uc11c\ub85c \ub2e4\ub978 \uc5ec\uc12f \uacc4\uc5f4\uc758 \uc720\uc801\uc744 \uc870\uc0ac\ud55c\ub2e4. " +
                       std::to_wstring(Bits(s.relics)) + L" / 6";
            case Truth:
                return L"\uc5ec\uc12f \uc720\ubb3c\uc758 \uae30\ub85d\uc744 \uc77d\uace0, \uc544\uc774\uc5d0\uac8c \ub3cc\uc544\uac00 \uc9c4\uc2e4\uc744 \uc804\ud55c\ub2e4.";
            case ShapedWorld:
                return L"\uacc4\uc2b9\uc790\ub4e4\uacfc \ub9fa\uc740 \uad00\uacc4\ub97c \ub3cc\uc544\ubcf4\uace0 \ub2e4\uc74c \uc2dc\ub300\uc758 \uc138\ub825\uc744 \ubaa8\uc740\ub2e4.";
            case Keeper:
                return L"\uc6b4\uba85\uc758 \ucd5c\ucd08 \ub2a5\ub825\uc790 \uc5d0\ub179\uc774 \uc9c0\ud0a8 \ubaa9\uc801\uc744 \ud655\uc778\ud55c\ub2e4.";
            case FirstGrave:
                return L"\uc5ec\uc12f \uc720\ubb3c\uc744 \uacb0\ud569\ud574 \uc88c\ud45c \ubc16\uc758 \ud0dc\ucd08\uc758 \ubb18\uc9c0\ub97c \uc5f0\ub2e4.";
            case Enoch:
                return L"\uacfc\uac70\uc758 \uc0b6\uacfc \uacc4\uc2b9\uc790\ub4e4\uc774 \uc9c0\ucf1c\ubcf4\ub294 \uac00\uc6b4\ub370 \uc5d0\ub179\uc744 \uc4f0\ub7ec\ub728\ub9b0\ub2e4.";
            case EndingChoice:
                return L"\uacc4\uc2b9\uc744 \ub05d\ub0b4\uac70\ub098, \uc774\uc5b4\ubc1b\uac70\ub098, \uc790\uc720\ub86d\uac8c \ub9cc\ub4e0\ub2e4.";
            default:
                return L"\uc138\uacc4\ub294 \uacc4\uc18d\ub41c\ub2e4. \uc774\uc804\uc758 \uc0ac\ub78c\uacfc \uc7a5\uc18c\ub294 \uc0ac\ub77c\uc9c0\uc9c0 \uc54a\ub294\ub2e4.";
        }
    }

    inline std::wstring Passage(const Game::World& w)
    {
        switch (w.story.stage)
        {
            case Prologue:
                return L"\uc804\uc5fc\ubcd1\uacfc \uae30\uadfc\uc774 \ubcc0\ubc29 \ub9c8\uc744\uc744 \uc9d3\ub204\ub978\ub2e4. \uc601\uc8fc\ub294 \uc138\uae08\uc744 \uac70\ub450\uace0, \uad50\ub2e8\uc740 \uc8fd\uc74c\uc774 \uc0c8 \uc0dd\uba85\uc744 \ubd80\ub978\ub2e4\uace0 \ub9d0\ud55c\ub2e4. \ub204\uad70\uac00\ub97c \ub3d5\uac70\ub098 \uc678\uba74\ud558\uba70 \ub2f9\uc2e0\uc758 \ud558\ub8e8\uac00 \uc2dc\uc791\ub41c\ub2e4.";
            case Successors:
                return L"\ub2f9\uc2e0\uc758 \uc0b6\uc740 \ub05d\ub0ac\ub2e4. \uadf8\ub7ec\ub098 \ub2f9\uc2e0\uc774 \ub0a8\uae34 \ubc29\ud5a5\uc740 \ub05d\ub098\uc9c0 \uc54a\uc558\ub2e4. \uc0c8\ub85c \ud0dc\uc5b4\ub09c \uc0ac\ub78c\uc740 \ub2f9\uc2e0\uc744 \uae30\uc5b5\ud558\uc9c0 \ubabb\ud55c\ub2e4. \ub2e4\ub9cc \uce7c\uc744 \uc950\ub294 \ubc84\ub987\uacfc \ud0c0\uc778\uc744 \uc9c0\ub098\uce58\uc9c0 \ubabb\ud558\ub294 \ub9c8\uc74c\uc774 \ub0af\uc775\ub2e4.";
            case Church:
                return L"\uc131\uacc4\uad50\ub2e8\uc740 \ubcd1\uc790\ub97c \ub3cc\ubcf4\uace0 \uc7a5\ub840\uc640 \ubc30\uae09\uc744 \ub9e1\ub294\ub2e4. \uadf8\ub7ec\ub098 \uc7a5\ubd80\uc5d0\ub294 \uac19\uc740 \ub9c8\uc744\ub85c \ud5a5\ud55c \uc2dd\ub7c9\uacfc \ubb34\uae30\uc758 \uae30\ub85d\uc774 \ud568\uaed8 \ub0a8\uc544 \uc788\ub2e4. \uc138 \uacf3\uc758 \uc99d\uac70\ub97c \ub300\uc870\ud574\uc57c \ud55c\ub2e4.";
            case Abelon:
                return L"\uc804\uc9c1 \uc7a5\uad70\uc774\uc790 \uad50\ub2e8\uc758 \uc9c0\ub3c4\uc790 \uc544\ubca8\ub860\uc740 \uad6c\ud638\ub97c \uc704\ud574 \uc804\uc7c1\uc744 \uc77c\uc73c\ucf30\ub2e4. \uadf8\ub294 \ubb3b\ub294\ub2e4. \ud55c \uc0ac\ub78c\uc774 \uc8fd\uc73c\uba74 \ub2e4\ub978 \uc0ac\ub78c\uc774 \ud0dc\uc5b4\ub09c\ub2e4. \uadf8\ub807\ub2e4\uba74 \uc8fd\uc74c\uc740 \uc815\ub9d0 \uc545\uc778\uac00?";
            case Child:
                return w.story.childId
                           ? L"\ub2f9\uc2e0\uc758 \uc8fd\uc74c\uc5d0\uc11c \ud0dc\uc5b4\ub09c \uc544\uc774\uac00 \ub9d0\ud55c\ub2e4. \ub2f9\uc2e0\uc774 \uc8fd\ub358 \uc21c\uac04\uc744 \ubcf8 \uc801 \uc788\uc5b4. \uae30\uc5b5\uc740 \uc804\ud574\uc9c0\uc9c0 \uc54a\ub294\ub2e4\ub294 \uad50\ub2e8\uc758 \uac00\ub974\uce68\uacfc \uc5b4\uae0b\ub09c\ub2e4."
                           : L"\uc544\ubca8\ub860\uc758 \uc8fd\uc74c \ub4a4\uc5d0\ub3c4 \uc0c8 \uc0dd\uba85\uc774 \ud0dc\uc5b4\ub0ac\ub2e4. \uc545\uc778\uc744 \uc8fd\uc774\ub294 \uc77c\uc870\ucc28 \ubc95\uce59\uc744 \uc787\ub294\ub2e4. \uadf8\uc758 \ubc00\uc11c\uc5d0\ub294 \ud0dc\uc5b4\ub098\uc9c0 \ub9d0\uc544\uc57c \ud560 \uc544\uc774\uc5d0 \uad00\ud55c \uc608\uc5b8\uc774 \ub0a8\uc544 \uc788\ub2e4.";
            case Ruins:
                return L"\uc544\uc774\ub294 \uae30\uc5b5\uc758 \ud30c\ud3b8\uc744 \uc9c0\ub2cc \uc720\uc77c\ud55c \uc608\uc678\ub2e4. \uc721\uccb4, \uac10\uac01, \uc815\uc2e0, \uc6b4\uba85, \ubb3c\uc9c8, \uc0dd\uba85\uc758 \uc720\uc801\uc5d0 \ucd5c\ucd08 \ub2a5\ub825\uc790\ub4e4\uc774 \ub0a8\uae34 \ud754\uc801\uc774 \uc788\ub2e4. \ub2a5\ub825\uc774 \uc544\ub2c8\ub77c \uc720\ubb3c\uc744 \ubaa8\uc544\uc57c \ud55c\ub2e4.";
            case Truth:
                return L"\uacc4\uc2b9\uc758 \ubc95\uce59\uc740 \uc790\uc5f0\uc758 \uc12d\ub9ac\uac00 \uc544\ub2c8\uc5c8\ub2e4. \uba78\uc885 \uc9c1\uc804\uc758 \uc778\ub958\ub97c \uad6c\ud558\ub824 \uc5ec\uc12f \ub2a5\ub825\uc790\uac00 \uc8fd\uc74c\uc758 \ud798\uc744 \uc0c8 \uc0dd\uba85\uc73c\ub85c \uc5ee\uc5c8\ub2e4. \uadf8\ub7ec\ub098 \uc0b6\uc758 \ubc29\ud5a5\uae4c\uc9c0 \uc804\ud574\uc838 \uc804\uc7c1\uacfc \uacf5\ud3ec\ub3c4 \ub2e4\uc74c \uc138\ub300\ub85c \uc774\uc5b4\uc84c\ub2e4.";
            case ShapedWorld:
                return L"\ub2f9\uc2e0\uc740 \uc138\uacc4\ub97c \ud0d0\ud5d8\ud558\uae30\ub9cc \ud55c \uac83\uc774 \uc544\ub2c8\uc5c8\ub2e4. \ub2e4\uc74c \uc2dc\ub300\ub97c \ub9cc\ub4e4\uace0 \uc788\uc5c8\ub2e4. \uad6c\ud638\ud68c, \uc6a9\ubcd1\ub2e8, \uc0c1\ub2e8\uacfc \ud0d0\uc0ac\ub2e8\uc5d0 \ub2f9\uc2e0\uc774 \ub0a8\uae34 \uc0ac\ub78c\ub4e4\uc774 \uc788\ub2e4. \uadf8\ub4e4\uc758 \uc120\ud0dd\uc740 \uc9c0\uae08 \ub9fa\uc740 \uad00\uacc4\uc5d0 \ub2ec\ub838\ub2e4.";
            case Keeper:
                return L"\ub2e4\uc12f \ucd5c\ucd08 \ub2a5\ub825\uc790\ub294 \uc8fd\uc5c8\uace0 \uc6b4\uba85\uc758 \ub2a5\ub825\uc790 \uc5d0\ub179\ub9cc \ub0a8\uc558\ub2e4. \uadf8\uc758 \ubaa9\uc801\uc740 \uc815\ubcf5\uc774 \uc544\ub2c8\ub77c \uc778\ub958\uc758 \uc874\uc18d\uc774\ub2e4. \ud589\ubcf5\uc740 \uc120\ud0dd\uc774\uc9c0\ub9cc \uc0dd\uc874\uc740 \uc758\ubb34\ub77c\uba70, \uadf8\ub294 \uc8fd\uc74c\uc774 \ubaa8\uc790\ub77c\uba74 \uac08\ub4f1\uc744 \ub9cc\ub4e4\uc5c8\ub2e4.";
            case FirstGrave:
                return L"\uc5ec\uc12f \uc720\ubb3c\uc774 \ubaa8\uc600\ub2e4. \ud0dc\ucd08\uc758 \ubb18\uc9c0\ub294 \uc9c0\ub3c4\uc0c1\uc758 \uc5b4\ub290 \uc88c\ud45c\uc5d0\ub3c4 \uc5c6\ub2e4. \ubb38 \ub108\uba38\uc5d0\ub294 \uacfc\uac70 \uc0ac\ub9dd \uc7a5\uc18c\uc758 \uc794\ud5a5\uacfc \ub2f9\uc2e0\uc774 \ub0a8\uae34 \uacc4\uc2b9\uc790\ub4e4\uc774 \uae30\ub2e4\ub9b0\ub2e4.";
            case Enoch:
                return L"\uc5d0\ub179\uc740 \uc6b4\uba85\uc73c\ub85c \ub2e4\uc74c \uc6c0\uc9c1\uc784\uc744 \uc77d\ub294\ub2e4. \uc804\ud22c\uac00 \uae4a\uc5b4\uc9c0\uba74 \ub2f9\uc2e0\uc758 \uc0b6\uc5d0\uc11c \uac00\uc7a5 \uc9d9\uc740 \ubc29\ud5a5\uc744 \ub418\ub3cc\ub824\uc900\ub2e4. \ub2f9\uc2e0\uc744 \uc2e0\ub8b0\ud558\ub294 \uacc4\uc2b9\uc790\ub294 \ub3d5\uace0, \uc801\ub300\ud558\ub294 \uacc4\uc2b9\uc790\ub294 \uadf8\uc758 \ud3b8\uc5d0 \uc120\ub2e4.";
            case EndingChoice:
                return L"\uc5d0\ub179\uc740 \uc4f0\ub7ec\uc84c\ub2e4. \uacc4\uc2b9\uc758 \ubc95\uce59\uc744 \uc9c0\ud0f1\ud558\ub358 \uc790\ub9ac\uac00 \ube44\uc5c8\ub2e4. \uc8fd\uc74c\uc744 \ub05d\uc73c\ub85c \ub3cc\ub824\ub193\uc744 \uac83\uc778\uac00. \uc655\uc88c\ub97c \uc774\uc5b4\ubc1b\uc744 \uac83\uc778\uac00. \uc544\ub2c8\uba74 \uc0c8 \uc0dd\uba85\uc774 \uacfc\uac70\uc758 \ubc29\ud5a5\uc744 \ub530\ub974\uc9c0 \uc54a\uac8c \ud560 \uac83\uc778\uac00.";
            default:
                return w.story.ending == Destroy
                           ? L"\ud55c \uc544\uc774\uac00 \ud0dc\uc5b4\ub09c\ub2e4. \uc774 \uc544\uc774\ub294 \ub204\uad6c\ub97c \uacc4\uc2b9\ud55c \uac78\uae4c\uc694? \uc544\ubb34\ub3c4. \uc790\uae30 \uc790\uc2e0\uc73c\ub85c \ud0dc\uc5b4\ub09c \uac70\uc57c. \uc774\uc81c \uc8fd\uc74c\uc740 \uc0c8 \uacc4\uc2b9\uc790\ub97c \ub9cc\ub4e4\uc9c0 \uc54a\uace0, \uc0b4\uc544\ub0a8\ub294 \ub300\uac00\ub294 \ub354 \ubb34\uac70\uc6cc\uc84c\ub2e4."
                       : w.story.ending == Successor
                           ? L"\ub2f9\uc2e0\uc774 \uc5d0\ub179\uc758 \uc655\uc88c\uc5d0 \uc549\ub294\ub2e4. \uc2ed \ub144, \ubc31 \ub144, \uc624\ubc31 \ub144. \ub208\ube5b\uc774 \uc870\uae08\uc529 \uadf8\ub97c \ub2ee\uc544\uac04\ub2e4. \uacc4\uc2b9\uc740 \uacc4\uc18d\ub418\uace0 \ub2f9\uc2e0\uc774 \ub0a8\uae34 \ubc29\ud5a5\uc774 \ub2e4\uc74c \uc2dc\ub300\uc5d0 \uc2a4\uba70\ub4e0\ub2e4."
                           : L"\ud558\ub098\uc758 \uc8fd\uc74c\uc740 \uc5ec\uc804\ud788 \ud558\ub098\uc758 \uc0dd\uba85\uc744 \ub9cc\ub4e0\ub2e4. \uadf8\ub7ec\ub098 \ubb3c\ub824\ubc1b\uc740 \ubc29\ud5a5\uc774 \uc6b4\uba85\uc744 \uacb0\uc815\ud558\uc9c0 \uc54a\ub294\ub2e4. \ub9c8\uc9c0\ub9c9 \uc120\ud0dd\uc740 \uc5b8\uc81c\ub098 \uc0b4\uc544 \uc788\ub294 \uc790\uc758 \uac83\uc774\ub2e4.";
        }
    }

    inline std::wstring NpcLine(const Game::World& w, const Game::Npc& n)
    {
        if (n.id == w.story.childId)
        {
            return w.story.relics == 63
                       ? L"\ubd24\uc5b4. \ud558\uc9c0\ub9cc \uadf8 \uae30\uc5b5\uc774 \ub0b4\uac00 \ub420 \ud544\uc694\ub294 \uc5c6\uc9c0? \ub0b4 \uc0b6\uc740 \ub0b4\uac00 \uace0\ub974\uace0 \uc2f6\uc5b4."
                       : L"\ub2f9\uc2e0\uc774 \uc8fd\ub358 \uc21c\uac04\uc744 \ubcf8 \uc801 \uc788\uc5b4. \ub2e4\ub978 \uc0ac\ub78c\uc740 \uae30\uc5b5\ud558\uc9c0 \ubabb\ud55c\ub2e4\ub294\ub370, \ub098\ub294 \uc65c \uadf8\uac78 \ubd24\uc744\uae4c?";
        }
        if (n.origin == 1)
        {
            return L"\uc0ac\ub78c\uc744 \uc9c0\ud0a4\uace0 \uc2f6\uc740\ub370, \uba85\ub839\ubd80\ud130 \ub0b4\ub9ac\uace0 \uc2f6\uc5b4\uc9d1\ub2c8\ub2e4. \uadf8 \ub9c8\uc74c\uc774 \uc5b4\ub514\uc11c \uc654\ub294\uc9c0\ub294 \ubaa8\ub985\ub2c8\ub2e4.";
        }
        if (n.freeWill || n.changed)
        {
            return L"\ucc98\uc74c \ud488\uc5c8\ub358 \ub9c8\uc74c\uacfc \ub2e4\ub978 \uae38\uc744 \uace8\ub790\uc5b4\uc694. \ub0b4\uac00 \ubb3c\ub824\ubc1b\uc740 \uac83\ub9cc\uc73c\ub85c \uc0b4 \ud544\uc694\ub294 \uc5c6\uc73c\ub2c8\uae4c\uc694.";
        }
        static const wchar_t* lines[] = {
            L"\uc774\uc0c1\ud558\uac8c \uce7c\uc744 \uc7a1\uc73c\uba74 \ub9c8\uc74c\uc774 \ud3b8\ud574\uc9d1\ub2c8\ub2e4. \ub204\uad6c\uc5d0\uac8c \ubc30\uc6e0\ub294\uc9c0\ub294 \ubaa8\ub974\uaca0\uc18c.",
            L"\uc65c \uadf8\ub7f0\uc9c0\ub294 \ubaa8\ub974\uaca0\uc9c0\ub9cc \ub204\uad70\uac00 \uace4\ub780\ud574\ud558\ub294 \ubaa8\uc2b5\uc744 \uadf8\ub0e5 \uc9c0\ub098\uce58\uc9c0 \ubabb\ud569\ub2c8\ub2e4.",
            L"\uc544\ubb34\ub3c4 \ucad3\uc544\uc624\uc9c0 \uc54a\ub294\ub370 \uc790\uafb8 \ub4a4\ub97c \ub3cc\uc544\ubd10\uc694. \ub5a0\uc624\ub974\ub294 \uae30\uc5b5\uc740 \uc5c6\uc5b4\uc694.",
            L"\ubb34\ub108\uc9c4 \ubb38 \ub4a4\uc5d0 \ubb34\uc5c7\uc774 \uc788\uc744\uae4c\uc694? \ubaa8\ub974\ub294 \uac83\uc744 \uadf8\ub0e5 \ub450\uae30\uac00 \uc5b4\ub824\uc6cc\uc694.",
            L"\ub204\uad70\uac00 \ub2e4\uccd0\uc57c \ud55c\ub2e4\uba74 \ub0b4\uac00 \uba3c\uc800 \uc11c\uace0 \uc2f6\uc5b4\uc694. \uc774\uc720\ub97c \ubb3b\ub294\ub2e4\uba74 \ubaa8\ub974\uaca0\uc5b4\uc694.",
            L"\uc870\uae08\ub9cc \ub354 \ubaa8\uc73c\uba74 \uc548\uc2ec\ud560 \uc218 \uc788\uc744 \uac83 \uac19\uc18c. \uadf8\ub7f0\ub370 \uadf8 \uc870\uae08\uc774 \ub05d\ub098\uc9c0 \uc54a\uc18c.",
            L"\ubb34\ub108\uc9c4 \uc9d1\uc5d0\ub3c4 \ub2e4\uc2dc \ubd88\uc744 \ucf24 \uc218 \uc788\ub2e4\uace0 \ubbff\uc5b4\uc694. \ub2f9\uc2e0\ub3c4 \uadf8\ub807\uac8c \uc0dd\uac01\ud558\ub098\uc694?"
        };
        return lines[n.archetype];
    }

    inline const wchar_t* DeathMessage(const Game::World& w)
    {
        return w.story.ending == Destroy
                   ? L"\uc774 \uc0b6\uc740 \ub05d\ub0ac\ub2e4. \uacc4\uc2b9\uc790\ub294 \ud0dc\uc5b4\ub098\uc9c0 \uc54a\uc558\uace0, \ub0a8\uc740 \uc774\ub4e4\uc774 \uc8fd\uc74c\uc744 \uae30\uc5b5\ud55c\ub2e4."
                   : L"\ub2f9\uc2e0\uc758 \uc0b6\uc740 \ub05d\ub0ac\ub2e4. \uadf8\ub7ec\ub098 \ub2f9\uc2e0\uc774 \ub0a8\uae34 \ubc29\ud5a5\uc740 \ub05d\ub098\uc9c0 \uc54a\uc558\ub2e4.";
    }

    inline void Talk(Game::World& w, Game::Npc& n)
    {
        n.met = true;
        if (w.story.stage == Successors && Count(w, 1) >= 3)
        {
            w.story.stage = Church;
        }
        if (n.id == w.story.childId)
        {
            if (w.story.stage == Child)
            {
                w.story.stage = Ruins;
            }
            if (w.story.relics == 63)
            {
                w.story.childComplete = true;
            }
        }
        w.dirty = true;
    }

    inline bool CanInvestigate(const Game::World& w, const Game::Site& site)
    {
        if (w.story.stage == Church)
        {
            int bit = site.kind == Game::Camp ? 1 : site.kind == Game::Wounded ? 2 : site.kind == Game::Relic ? 4 : 0;
            return bit && !(w.story.clues & bit);
        }
        return w.story.stage == Ruins && site.kind == Game::Relic && !(w.story.relics & (1 << site.ability));
    }

    inline std::wstring Investigate(Game::World& w, const Game::Site& site)
    {
        if (!CanInvestigate(w, site))
        {
            return L"\uc774\ubbf8 \ud655\uc778\ud55c \ud754\uc801\uc774\ub2e4.";
        }
        std::wstring message;
        if (w.story.stage == Church)
        {
            int bit = site.kind == Game::Camp ? 1 : site.kind == Game::Wounded ? 2 : 4;
            w.story.clues |= bit;
            message =
                bit == 1
                    ? L"\ubc30\uae09 \uc7a5\ubd80: \uace1\ubb3c\uc744 \uac10\ucd98 \ub0a0\uc9dc\uc640 \uc6a9\ubcd1\uc744 \uace0\uc6a9\ud55c \ub0a0\uc9dc\uac00 \uac19\ub2e4."
                : bit == 2
                    ? L"\ud589\uc778\uc758 \uc99d\uc5b8: \uad50\ub2e8\uc740 \uc57d\uc744 \uac00\uc9c0\uace0 \uc788\uc5c8\uc9c0\ub9cc \ub9c8\uc744\uc758 \ubb38\uc744 \uc5f4\uc9c0 \uc54a\uc558\ub2e4."
                    : L"\uad50\ub2e8\uc758 \ubc00\uc11c: \uc8fd\uc74c\uc744 \ub298\ub824 \ud0c4\uc0dd\uc744 \ub298\ub9ac\uace0, \uacc4\uc2b9\uc744 \uc720\uc9c0\ud558\ub77c.";
            if (w.story.clues == 7)
            {
                w.story.stage = Abelon;
            }
        }
        else
        {
            w.story.relics |= 1 << site.ability;
            message =
                std::wstring(Game::AbilityName(site.ability)) +
                L"\uc758 \uc720\ubb3c\uc744 \ucc3e\uc558\ub2e4. \uc5ec\uc12f \ud798\uc740 \uba78\uc885\uc744 \ub9c9\uae30 \uc704\ud574 \ud558\ub098\uac00 \ub418\uc5c8\ub2e4.";
            if (w.story.relics == 63)
            {
                w.story.stage = Truth;
            }
        }
        w.player.echo[3] += 8;
        w.dirty = true;
        return message;
    }

    inline bool CanAdvance(const Game::World& w)
    {
        return w.story.stage == Truth || w.story.stage == ShapedWorld || w.story.stage == Keeper;
    }

    inline void Advance(Game::World& w)
    {
        if (CanAdvance(w))
        {
            ++w.story.stage;
            w.dirty = true;
        }
    }

    inline int AvailableBattle(const Game::World& w)
    {
        if (w.story.stage == Prologue && Bits(w.story.villageChoices) >= 2)
        {
            return 1;
        }
        if (w.story.stage == Abelon)
        {
            return 2;
        }
        if ((w.story.stage == FirstGrave || w.story.stage == Enoch) && w.story.relics == 63)
        {
            return 3;
        }
        return 0;
    }

    inline void PrepareMemories(Game::World& w)
    {
        auto& battle = w.storyBattle;
        battle.memories.clear();
        if (battle.encounter != 3)
        {
            return;
        }
        for (auto i = w.story.deaths.rbegin(); i != w.story.deaths.rend() && battle.memories.size() < 8; ++i)
        {
            LevelOne::Point p(std::fmod(i->x, 480), std::fmod(i->y, 480));
            if (!LevelOne::Walkable(battle, p, 10))
            {
                p = LevelOne::Point(0, (int)battle.memories.size() * 48 - 144);
            }
            battle.memories.push_back(p);
        }
    }

    inline void BeginBattle(Game::World& w, unsigned seed)
    {
        int mode = AvailableBattle(w);
        if (!mode)
        {
            return;
        }
        auto& battle = w.storyBattle;
        if (battle.encounter != mode || battle.phase != LevelOne::Fighting || !battle.seed)
        {
            if (w.levelOne.level > battle.level || (w.levelOne.level == battle.level && w.levelOne.xp > battle.xp))
            {
                battle.level = w.levelOne.level;
                battle.xp = w.levelOne.xp;
            }
            battle.weapon = std::max(battle.weapon, w.levelOne.weapon);
            LevelOne::Start(battle, seed);
            battle.encounter = mode;
            battle.dominantTrait = Dominant(LifeTraits(w));
            battle.enemies.clear();
            battle.drops.clear();
            if (mode >= 2)
            {
                battle.bossState = 2;
                LevelOne::SpawnEnemy(
                    battle,
                    LevelOne::Boss,
                    LevelOne::CenterOf(LevelOne::Center, LevelOne::Center + 9)
                );
                auto& boss = battle.enemies.back();
                boss.health = boss.maxHealth = std::min(
                    9500.f,
                    std::max(mode == 2 ? 900.f : 1400.f, LevelOne::Damage(battle) * (mode == 2 ? 35 : 60))
                );
                if (mode == 3)
                {
                    battle.allies = std::min(8, Count(w, 4));
                    battle.opponents = std::min(8, Count(w, 5));
                }
            }
            else
            {
                LevelOne::SpawnEnemy(battle, LevelOne::Raider, LevelOne::Point(144, 0));
                LevelOne::SpawnEnemy(battle, LevelOne::Archer, LevelOne::Point(0, 192));
            }
            LevelOne::AddDrop(battle, LevelOne::Point(48, 0), LevelOne::Healing, 2);
        }
        battle.active = true;
        w.levelOne.active = false;
        if (mode == 3)
        {
            w.story.stage = Enoch;
        }
        LevelOne::Notice(battle, LevelOne::AreaName(battle));
        PrepareMemories(w);
        w.dirty = true;
    }

    inline void FinishBattle(Game::World& w)
    {
        auto& battle = w.storyBattle;
        if (battle.phase != LevelOne::Cleared)
        {
            return;
        }
        if (battle.encounter == 2 && w.story.stage == Abelon)
        {
            Game::Npc born;
            born.id = (int)w.npcs.size() + 1;
            born.origin = 1;
            born.archetype = 4;
            born.echo[0] = 18;
            born.echo[4] = 24;
            born.ability = (Game::Ability)(Game::Hash(born.id, 73, 101) % Game::AbilityCount);
            born.home = born.p = Game::Vec(85, 55);
            w.npcs.push_back(born);
            w.Record(Game::Born);
            w.story.stage = Child;
            LevelOne::Notice(
                battle,
                L"\uc544\ubca8\ub860\uc740 \uc8fd\uc5c8\ub2e4. \uc138\uacc4 \uc5b4\ub518\uac00\uc5d0\uc11c \uc0c8 \uc0dd\uba85\uc774 \ud0dc\uc5b4\ub0ac\ub2e4."
            );
        }
        else if (battle.encounter == 3 && w.story.stage == Enoch)
        {
            w.story.stage = EndingChoice;
            LevelOne::Notice(
                battle,
                L"\uc5d0\ub179\uc740 \uc4f0\ub7ec\uc84c\ub2e4. \uc774\uc81c \ubc95\uce59\uc758 \uc55e\ub0a0\uc744 \uc120\ud0dd\ud574\uc57c \ud55c\ub2e4."
            );
        }
        w.dirty = true;
    }

    inline void ChooseEnding(Game::World& w, int ending)
    {
        if (w.story.stage != EndingChoice || ending < Destroy || ending > Freedom ||
            (ending == Freedom && !FreedomReady(w)))
        {
            return;
        }
        w.story.worldTrait = Dominant(LifeTraits(w));
        w.story.ending = ending;
        w.story.stage = Aftermath;
        w.story.nextBirth = w.time + 180;
        w.dirty = true;
    }

    inline void UpdateWorld(Game::World& w)
    {
        if (w.story.ending == Successor)
        {
            int trait = w.story.worldTrait;
            auto site = Game::GetSite(Game::Chunk(w.player.p.x), Game::Chunk(w.player.p.y));
            if (!w.states.count(site.key))
            {
                Game::SiteState state;
                if (site.kind == Game::Camp && (trait == 1 || trait == 4 || trait == 6))
                {
                    state.stage = 1;
                }
                if (site.kind == Game::Cache && trait == 5)
                {
                    state.stage = 1;
                }
                if (site.kind == Game::Wounded && (trait == 0 || trait == 2))
                {
                    state.stage = 2;
                }
                w.states.emplace(site.key, state);
                w.dirty = true;
            }
        }
        if (w.story.ending != Destroy || w.time < w.story.nextBirth || w.npcs.size() >= 200000)
        {
            return;
        }
        Game::Npc child;
        child.id = (int)w.npcs.size() + 1;
        child.origin = 2;
        child.freeWill = true;
        child.archetype = Game::Hash(child.id, 12, 7) % 7;
        child.ability = (Game::Ability)(Game::Hash(child.id, 73, 101) % Game::AbilityCount);
        child.home = child.p = Game::GetSite(Game::Chunk(w.player.p.x), Game::Chunk(w.player.p.y)).p;
        w.npcs.push_back(child);
        w.story.nextBirth = w.time + 180;
        w.dirty = true;
    }
}
