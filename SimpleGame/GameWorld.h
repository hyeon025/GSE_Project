#pragma once
#include <algorithm>
#include <string>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <vector>
#include <ShlObj.h>
#include "LevelOne.h"
#include "StoryState.h"

namespace Game
{
    enum Ability
    {
        Body,
        Sense,
        Mind,
        Fate,
        Matter,
        Life,
        AbilityCount
    };

    enum SiteKind
    {
        Camp,
        Wounded,
        Herb,
        Cache,
        Relic,
        Shrine
    };

    enum Event
    {
        Awakened,
        Gathered,
        Salvaged,
        Repaired,
        Rested,
        Healed,
        Sacrificed,
        Robbed,
        Investigated,
        Offered,
        Born,
        Befriended,
        Learned,
        Remembered,
        FieldCleared
    };

    struct Vec
    {
        double x = 0, y = 0;

        Vec(double a = 0, double b = 0)
            : x(a),
              y(b)
        {
        }
    };

    inline double Distance(Vec a, Vec b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    inline unsigned Hash(int x, int y, int salt)
    {
        unsigned h = (2166136261u ^ (unsigned)x) * 16777619u;
        h = (h ^ (unsigned)y) * 16777619u;
        h = (h ^ (unsigned)salt) * 16777619u;
        h ^= h >> 13;
        return h * 1274126177u;
    }

    inline float Road(Vec p)
    {
        return (float)std::min(
            std::abs(p.x - p.y) * 0.7071,
            std::min(std::abs(p.x + p.y + 128) * 0.7071, std::abs(p.y - std::sin(p.x / 145) * 100 + 320) * 0.78)
        );
    }

    typedef std::pair<int, int> Key;

    struct Site
    {
        Key key;
        Vec p;
        SiteKind kind;
        Ability ability;
    };

    struct SiteState
    {
        int stage = 0;
        double readyAt = 0;
        bool trained = false;
    };

    struct Npc
    {
        Vec p, home;
        int id = 0, archetype = 0, trust = 0;
        Ability ability = Body;
        std::array<int, 7> echo = {};
        int originDeathId = 0, origin = 0, relationship = 0, power = 20;
        bool met = false, saved = false, changed = false, freeWill = false;
    };

    struct Player
    {
        Vec p, checkpoint;
        float health = 100, stamina = 100;
        int bread = 3, herbs = 1, wood = 0, fragments = 0;
        Ability ability = Fate;
        std::array<int, AbilityCount> training = {};
        std::array<int, 7> echo = {};
        double exploration = 0;
    };

    struct World
    {
        Player player;
        LevelOne::Session levelOne;
        LevelOne::Session storyBattle;
        Story::State story;
        int deaths = 0;
        double time = 0;
        std::map<Key, SiteState> states;
        std::vector<Npc> npcs;
        std::vector<int> events;
        bool dirty = false;

        World()
        {
            storyBattle.active = false;
        }

        void Record(Event e)
        {
            events.push_back(e);
            if (story.stage == Story::Prologue)
            {
                if (e == Healed || e == Sacrificed)
                {
                    story.villageChoices |= 1;
                }
                if (e == Robbed || e == Salvaged)
                {
                    story.villageChoices |= 2;
                }
                if (e == Repaired || e == Rested)
                {
                    story.villageChoices |= 4;
                }
            }
            dirty = true;
        }
    };

    inline Site GetSite(int cx, int cy)
    {
        unsigned h = Hash(cx, cy, 91);
        Site s;
        s.key = Key(cx, cy);
        s.p = Vec(cx * 320.0 + 80 + h % 160, cy * 320.0 + 80 + (h / 173) % 160);
        s.kind = (SiteKind)(h % 6);
        s.ability = (Ability)((h / 13) % AbilityCount);
        if (cx == 0 && cy == 0)
        {
            s.p = Vec(55, 55);
            s.kind = Camp;
        }
        if (cx == -1 && cy == 0)
        {
            s.p = Vec(-75, 30);
            s.kind = Wounded;
            s.ability = Matter;
        }
        if (cx == 0 && cy == -1)
        {
            s.p = Vec(35, -100);
            s.kind = Relic;
        }
        if (cx == -1 && cy == -1)
        {
            s.p = Vec(-110, -80);
            s.kind = Herb;
        }
        if (cx == 1 && cy == 0)
        {
            s.p = Vec(190, 20);
            s.kind = Cache;
        }
        if (cx == 0 && cy == 1)
        {
            s.p = Vec(70, 230);
            s.kind = Shrine;
        }
        for (int i = 0; i < AbilityCount; ++i)
        {
            if (s.key == Story::RuinKeys()[i])
            {
                s.kind = Relic;
                s.ability = (Ability)i;
            }
        }
        return s;
    }

    inline int Chunk(double p)
    {
        return (int)std::floor(p / 320.0);
    }

    inline SiteState State(const World& w, Key key)
    {
        auto i = w.states.find(key);
        return i == w.states.end() ? SiteState() : i->second;
    }

    inline std::vector<Site> Sites(Vec p, int radius)
    {
        std::vector<Site> result;
        int cx = Chunk(p.x), cy = Chunk(p.y);
        for (int y = cy - radius; y <= cy + radius; ++y)
        {
            for (int x = cx - radius; x <= cx + radius; ++x)
            {
                result.push_back(GetSite(x, y));
            }
        }
        return result;
    }

    inline bool Obstacle(int tx, int ty)
    {
        Vec p(tx * 32.0 + 16, ty * 32.0 + 16);
        if (Road(p) < 50 || Hash(tx, ty, 37) % 100 >= 14)
        {
            return false;
        }
        // Clear generous approaches around deterministic interaction sites.
        int cx = Chunk(p.x), cy = Chunk(p.y);
        for (int y = cy - 1; y <= cy + 1; ++y)
        {
            for (int x = cx - 1; x <= cx + 1; ++x)
            {
                if (Distance(GetSite(x, y).p, p) < 66)
                {
                    return false;
                }
            }
        }
        return true;
    }

    inline bool Walkable(Vec p)
    {
        int tx = (int)std::floor(p.x / 32), ty = (int)std::floor(p.y / 32);
        for (int y = ty - 1; y <= ty + 1; ++y)
        {
            for (int x = tx - 1; x <= tx + 1; ++x)
            {
                if (Obstacle(x, y) && Distance(p, Vec(x * 32.0 + 16, y * 32.0 + 16)) < 20)
                {
                    return false;
                }
            }
        }
        return true;
    }

    inline bool Adjacent(Ability a, Ability b)
    {
        int d = std::abs((int)a - (int)b);
        return d == 1 || d == 5;
    }

    inline const wchar_t* AbilityName(Ability a)
    {
        static const wchar_t* names[] =
            {L"\uc721\uccb4", L"\uac10\uac01", L"\uc815\uc2e0", L"\uc6b4\uba85", L"\ubb3c\uc9c8", L"\uc0dd\uba85"};
        return names[(int)a];
    }

    inline const wchar_t* Personality(int a)
    {
        static const wchar_t* names[] = {
            L"\uacbd\uacc4\ud558\ub294 \uc6a9\ubcd1",
            L"\uc790\ube44\ub85c\uc6b4 \uc21c\ub840\uc790",
            L"\ubd88\uc548\ud55c \uc740\ub454\uc790",
            L"\ub5a0\ub3c4\ub294 \ud0d0\uad6c\uc790",
            L"\ud5cc\uc2e0\ud558\ub294 \uc218\ud638\uc790",
            L"\uc695\uc2ec \ub9ce\uc740 \uc218\uc9d1\uac00",
            L"\ud76c\ub9dd\uc744 \ud488\uc740 \ubc29\ub791\uc790"
        };
        return names[a];
    }

    inline const wchar_t* EventText(int e)
    {
        static const wchar_t* text[] = {
            L"\uc7ac\uc758 \uae38\uc5d0\uc11c \ub2e4\uc2dc \ub208\uc744 \ub5b4\ub2e4.",
            L"\uc57d\ucd08\ub97c \ucc44\uc9d1\ud588\ub2e4. \ubfcc\ub9ac\ub294 \ub0a8\uaca8\ub450\uc5c8\ub2e4.",
            L"\ubc84\ub824\uc9c4 \uc218\ub808\uc5d0\uc11c \uc2dd\ub7c9\uacfc \ubaa9\uc7ac\ub97c \ucc3e\uc558\ub2e4.",
            L"\uaebc\uc9c4 \uc57c\uc601\uc9c0\uc5d0 \ub2e4\uc2dc \ubd88\uc774 \ub4e4\uc5b4\uc654\ub2e4.",
            L"\ubd88\uac00\uc5d0\uc11c \uc2dd\ub7c9\uc744 \ub098\ub204\uace0 \uc0c1\ucc98\ub97c \ub3cc\ubd24\ub2e4.",
            L"\ubd80\uc0c1\ub2f9\ud55c \ud589\uc778\uc744 \uce58\ub8cc\ud588\ub2e4.",
            L"\uc0c1\ucc98\ub97c \ubb34\ub985\uc4f0\uace0 \ud589\uc778\uc744 \uad6c\uc870\ud588\ub2e4.",
            L"\ud589\uc778\uc758 \uc2dd\ub7c9\uc744 \ube7c\uc557\uc558\ub2e4. \uadf8\ub294 \uc785\uc744 \ub2eb\uc558\ub2e4.",
            L"\ud3d0\ud5c8\uc5d0\uc11c \uc624\ub798\ub41c \uae30\uc5b5\uc758 \ud30c\ud3b8\uc744 \ucc3e\uc558\ub2e4.",
            L"\uc81c\ub2e8\uc5d0 \ud30c\ud3b8\uc744 \ubc14\uccd0 \uc791\uc740 \ucd1b\ubd88\uc744 \ub0a8\uacbc\ub2e4.",
            L"\ud558\ub098\uc758 \uc8fd\uc74c\uc774 \uc0c8\ub85c\uc6b4 \uc0dd\uba85\uc744 \ub0a8\uacbc\ub2e4.",
            L"\uacc4\uc2b9\uc790\uc640 \ube75\uc744 \ub098\ub234\ub2e4.",
            L"\uc778\uc811 \uacc4\uc5f4\uc758 \uc57d\ud55c \ub2a5\ub825\uc744 \ubc30\uc6e0\ub2e4.",
            L"\uacc4\uc2b9\uc790\uc758 \uc774\uc57c\uae30\uc5d0 \uadc0\ub97c \uae30\uc6b8\uc600\ub2e4."
        };
        if (e == FieldCleared)
        {
            return L"\uc7bf\ube5b \uacbd\uc791\uc9c0\uc758 \ud30c\uc218\uafbc\uc744 \uc4f0\ub7ec\ub728\ub838\ub2e4. \ub2e4\uc2dc \ubd88\ube5b\uc774 \ub3cc\uc544\uc654\ub2e4.";
        }
        return text[e];
    }

    inline void Die(World& w, int cause = Story::Offering)
    {
        Story::DeathRecord record;
        record.id = ++w.deaths;
        record.cause = cause;
        record.stage = w.story.stage;
        record.lastAction = w.events.empty() ? Awakened : w.events.back();
        record.traits = w.player.echo;
        Vec location = w.player.p;
        if (w.storyBattle.active)
        {
            location = Vec(w.storyBattle.p.x, w.storyBattle.p.y);
        }
        else if (w.levelOne.active)
        {
            location = Vec(w.levelOne.p.x, w.levelOne.p.y);
        }
        record.x = location.x;
        record.y = location.y;
        for (const auto& npc : w.npcs)
        {
            if (!w.levelOne.active && !w.storyBattle.active && Distance(npc.p, w.player.p) < 100)
            {
                record.nearbyNpc = npc.id;
                break;
            }
        }
        Npc n;
        n.id = (int)w.npcs.size() + 1;
        n.originDeathId = w.deaths;
        n.echo = w.player.echo;
        if (w.story.ending == Story::Freedom)
        {
            n.freeWill = true;
            n.echo.fill(0);
            n.echo[Hash(n.id, 27, 83) % 7] = 12;
        }
        else if (w.story.ending == Story::Successor)
        {
            n.echo[w.story.worldTrait] += 12;
        }
        n.archetype = 6;
        int best = 0;
        for (int i = 0; i < 7; ++i)
        {
            if (n.echo[i] > best)
            {
                best = n.echo[i];
                n.archetype = i;
            }
        }
        // Innate ability is fixed at birth, independently of inherited personality.
        n.ability = (Ability)(Hash(n.id, 73, 101) % AbilityCount);
        n.p = n.home = w.player.p;
        if (w.story.stage == Story::Child && w.story.childId == 0)
        {
            w.story.childId = n.id;
            n.p = n.home = Vec(35, 55);
        }
        n.power = std::min(100, 20 + best);
        if (w.story.ending != Story::Destroy)
        {
            w.npcs.push_back(n);
            record.successorId = n.id;
            w.Record(Born);
        }
        else
        {
            w.player.bread /= 2;
            w.player.fragments /= 2;
            w.levelOne.xp = 0;
            w.storyBattle.xp = 0;
        }
        w.story.deaths.push_back(record);
        if (w.story.stage == Story::Prologue)
        {
            w.story.stage = Story::Successors;
        }
        w.player.p = w.player.checkpoint;
        w.player.health = w.player.stamina = 100;
        if (w.story.ending == Story::Destroy)
        {
            w.player.health = 50;
        }
        w.player.echo.fill(0);
        w.player.exploration = 0;
        w.dirty = true;
    }

    inline std::wstring SavePath()
    {
        wchar_t path[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path)))
        {
            return L"";
        }
        std::wstring root = std::wstring(path) + L"\\GSE_Project";
        if (!CreateDirectoryW(root.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            return L"";
        }
        return root + L"\\world_v1.txt";
    }

    inline bool Save(World& w)
    {
        std::wstring path = SavePath();
        if (path.empty())
        {
            return false;
        }
        std::wstring temporary = path + L".tmp";
        std::ofstream f(temporary.c_str(), std::ios::trunc);
        if (!f)
        {
            return false;
        }
        const Player& p = w.player;
        f << std::setprecision(17) << "GSE_WORLD 3\n" << w.time << ' ' << w.deaths << '\n';
        f << p.p.x << ' ' << p.p.y << ' ' << p.checkpoint.x << ' ' << p.checkpoint.y << ' ' << p.health << ' '
          << p.stamina << ' ' << p.bread << ' ' << p.herbs << ' ' << p.wood << ' ' << p.fragments << ' '
          << (int)p.ability << ' ' << p.exploration << '\n';
        for (int x : p.echo)
        {
            f << x << ' ';
        }
        f << '\n';
        for (int x : p.training)
        {
            f << x << ' ';
        }
        f << '\n';
        f << w.states.size() << '\n';
        for (const auto& e : w.states)
        {
            f << e.first.first << ' ' << e.first.second << ' ' << e.second.stage << ' ' << e.second.readyAt << ' '
              << e.second.trained << '\n';
        }
        f << w.npcs.size() << '\n';
        for (const auto& n : w.npcs)
        {
            f << n.id << ' ' << n.p.x << ' ' << n.p.y << ' ' << n.home.x << ' ' << n.home.y << ' ' << n.archetype << ' '
              << (int)n.ability << ' ' << n.trust << ' ';
            for (int e : n.echo)
            {
                f << e << ' ';
            }
            f << n.originDeathId << ' ' << n.origin << ' ' << n.relationship << ' ' << n.power << ' ' << n.met << ' '
              << n.saved << ' ' << n.changed << ' ' << n.freeWill;
            f << '\n';
        }
        f << w.events.size() << '\n';
        for (int e : w.events)
        {
            f << e << ' ';
        }
        f << '\n';
        // Keep the original world fields in order so version-one saves remain readable.
        LevelOne::Write(f, w.levelOne, true);
        Story::Write(f, w.story);
        LevelOne::Write(f, w.storyBattle, true);
        f.flush();
        bool ok = f.good();
        f.close();
        ok = ok && !f.fail();
        if (!ok || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            return false;
        }
        w.dirty = false;
        return true;
    }

    inline bool FinitePosition(Vec p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) < 1e10 && std::abs(p.y) < 1e10;
    }

    inline bool Load(World& w, bool& damaged)
    {
        damaged = false;
        std::wstring path = SavePath();
        if (path.empty())
        {
            return false;
        }
        std::ifstream f(path.c_str());
        if (!f)
        {
            damaged = GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
            return false;
        }
        damaged = true;
        World result;
        Player& p = result.player;
        std::string magic;
        int version = 0, ability = 0;
        if (!(f >> magic >> version) || magic != "GSE_WORLD" || version < 1 || version > 3)
        {
            return false;
        }
        f >> result.time >> result.deaths;
        f >> p.p.x >> p.p.y >> p.checkpoint.x >> p.checkpoint.y >> p.health >> p.stamina >> p.bread >> p.herbs >>
            p.wood >> p.fragments >> ability >> p.exploration;
        if (!f || !FinitePosition(p.p) || !FinitePosition(p.checkpoint) || !std::isfinite(result.time) ||
            result.time < 0 || result.deaths < 0 || result.deaths > 100000 || ability < 0 || ability >= AbilityCount ||
            !(p.health > 0 && p.health <= 100) || !(p.stamina >= 0 && p.stamina <= 100) ||
            !std::isfinite(p.exploration) || p.exploration < 0 || p.exploration >= 160 || p.bread < 0 || p.herbs < 0 ||
            p.wood < 0 || p.fragments < 0)
        {
            return false;
        }
        p.ability = (Ability)ability;
        for (int& e : p.echo)
        {
            f >> e;
            if (e < 0)
            {
                return false;
            }
        }
        for (int& e : p.training)
        {
            f >> e;
            if (e < 0 || e > 1)
            {
                return false;
            }
        }
        for (int i = 0; i < AbilityCount; ++i)
        {
            if (p.training[i] && !Adjacent(p.ability, (Ability)i))
            {
                return false;
            }
        }
        size_t count = 0;
        if (!(f >> count) || count > 1000000)
        {
            return false;
        }
        for (size_t i = 0; i < count; ++i)
        {
            Key k;
            SiteState s;
            f >> k.first >> k.second >> s.stage >> s.readyAt >> s.trained;
            if (!f || s.stage < 0 || s.stage > 2 || !std::isfinite(s.readyAt) || s.readyAt < 0)
            {
                return false;
            }
            if (!result.states.insert(std::make_pair(k, s)).second)
            {
                return false;
            }
        }
        if (!(f >> count) || count > 200000 || (version < 3 && count != (size_t)result.deaths))
        {
            return false;
        }
        std::set<int> originDeaths;
        for (size_t i = 0; i < count; ++i)
        {
            Npc n;
            f >> n.id >> n.p.x >> n.p.y >> n.home.x >> n.home.y >> n.archetype >> ability >> n.trust;
            if (!f || n.id != (int)i + 1 || !FinitePosition(n.p) || !FinitePosition(n.home) || n.archetype < 0 ||
                n.archetype > 6 || ability < 0 || ability >= AbilityCount || n.trust < 0 || n.trust > 3)
            {
                return false;
            }
            n.ability = (Ability)ability;
            for (int& e : n.echo)
            {
                f >> e;
                if (e < 0)
                {
                    return false;
                }
            }
            if (version >= 3)
            {
                f >> n.originDeathId >> n.origin >> n.relationship >> n.power >> n.met >> n.saved >> n.changed >>
                    n.freeWill;
                if (!f || n.originDeathId < 0 || n.originDeathId > result.deaths || n.origin < 0 || n.origin > 2 ||
                    n.relationship < -100 || n.relationship > 100 || n.power < 0 || n.power > 100 ||
                    (n.origin == 0) != (n.originDeathId > 0))
                {
                    return false;
                }
            }
            else
            {
                n.originDeathId = n.id;
                n.relationship = n.trust * 25;
            }
            if (n.origin == 0 && !originDeaths.insert(n.originDeathId).second)
            {
                return false;
            }
            result.npcs.push_back(n);
        }
        if (!(f >> count) || count > 1000000)
        {
            return false;
        }
        for (size_t i = 0; i < count; ++i)
        {
            int e = -1;
            f >> e;
            if (e < 0 || e > FieldCleared)
            {
                return false;
            }
            result.events.push_back(e);
        }
        if (!f)
        {
            return false;
        }
        if (version >= 2 && !LevelOne::Read(f, result.levelOne, version >= 3))
        {
            return false;
        }
        if (version >= 3)
        {
            if (!Story::Read(f, result.story) || !LevelOne::Read(f, result.storyBattle, true) ||
                (result.levelOne.active && result.storyBattle.active) || result.levelOne.encounter != 0 ||
                result.story.childId > (int)result.npcs.size())
            {
                return false;
            }
            if (result.story.childId && result.npcs[result.story.childId - 1].origin != 0)
            {
                return false;
            }
            for (const auto& death : result.story.deaths)
            {
                if (death.id > result.deaths || death.successorId > (int)result.npcs.size() ||
                    death.nearbyNpc > (int)result.npcs.size() ||
                    (death.successorId && result.npcs[death.successorId - 1].originDeathId != death.id))
                {
                    return false;
                }
            }
        }
        else
        {
            result.story.stage = result.deaths ? Story::Successors : Story::Prologue;
            result.story.villageChoices = result.levelOne.completedOnce ? 7 : 0;
            // Older saves have no death history; never invent missing memories.
        }
        w = result;
        damaged = false;
        return true;
    }
}
