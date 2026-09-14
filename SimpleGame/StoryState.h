#pragma once
#include <array>
#include <cmath>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include <utility>

namespace Story
{
    enum Stage
    {
        Prologue,
        Successors,
        Church,
        Abelon,
        Child,
        Ruins,
        Truth,
        ShapedWorld,
        Keeper,
        FirstGrave,
        Enoch,
        EndingChoice,
        Aftermath
    };

    enum Ending
    {
        None,
        Destroy,
        Successor,
        Freedom
    };

    enum Cause
    {
        Offering,
        Ruin,
        Field,
        VillageRaid,
        ChurchBattle,
        FinalBattle
    };

    inline const std::array<std::pair<int, int>, 6>& RuinKeys()
    {
        static const std::array<std::pair<int, int>, 6> keys = {{{-2, 0}, {2, 0}, {0, -2}, {0, 2}, {-2, -2}, {2, 2}}};
        return keys;
    }

    struct DeathRecord
    {
        int id = 0, successorId = 0;
        int cause = Offering, stage = Prologue, lastAction = 0, nearbyNpc = 0;
        double x = 0, y = 0;
        std::array<int, 7> traits = {};
    };

    struct State
    {
        int stage = Prologue;
        int villageChoices = 0, clues = 0, relics = 0, childId = 0;
        int ending = None, worldTrait = 6;
        bool childComplete = false;
        double nextBirth = 0;
        std::vector<DeathRecord> deaths;
    };

    inline int Bits(int mask)
    {
        int result = 0;
        for (; mask; mask >>= 1)
        {
            result += mask & 1;
        }
        return result;
    }

    inline int Dominant(const std::array<int, 7>& traits)
    {
        int result = 6;
        for (int i = 0; i < 7; ++i)
        {
            if (traits[i] > traits[result])
            {
                result = i;
            }
        }
        return result;
    }

    inline void Write(std::ostream& out, const State& s)
    {
        out << "STORY 1 " << s.stage << ' ' << s.villageChoices << ' ' << s.clues << ' ' << s.relics << ' ' << s.childId
            << ' ' << s.ending << ' ' << s.worldTrait << ' ' << s.childComplete << ' ' << s.nextBirth << '\n';
        out << s.deaths.size() << '\n';
        for (const auto& d : s.deaths)
        {
            out << d.id << ' ' << d.successorId << ' ' << d.cause << ' ' << d.stage << ' ' << d.lastAction << ' '
                << d.nearbyNpc << ' ' << d.x << ' ' << d.y;
            for (int trait : d.traits)
            {
                out << ' ' << trait;
            }
            out << '\n';
        }
    }

    inline bool Read(std::istream& in, State& s)
    {
        std::string magic;
        int version = 0;
        in >> magic >> version >> s.stage >> s.villageChoices >> s.clues >> s.relics >> s.childId >> s.ending >>
            s.worldTrait >> s.childComplete >> s.nextBirth;
        if (!in || magic != "STORY" || version != 1 || s.stage < Prologue || s.stage > Aftermath ||
            s.villageChoices < 0 || s.villageChoices > 7 || s.clues < 0 || s.clues > 7 || s.relics < 0 ||
            s.relics > 63 || s.childId < 0 || s.ending < None || s.ending > Freedom || s.worldTrait < 0 ||
            s.worldTrait > 6 || !std::isfinite(s.nextBirth) || s.nextBirth < 0 ||
            (s.stage == Aftermath) != (s.ending != None))
        {
            return false;
        }
        size_t count = 0;
        if (!(in >> count) || count > 100000)
        {
            return false;
        }
        int previous = 0;
        for (size_t i = 0; i < count; ++i)
        {
            DeathRecord d;
            in >> d.id >> d.successorId >> d.cause >> d.stage >> d.lastAction >> d.nearbyNpc >> d.x >> d.y;
            if (!in || d.id <= previous || d.successorId < 0 || d.cause < Offering || d.cause > FinalBattle ||
                d.stage < Prologue || d.stage > Aftermath || d.lastAction < 0 || d.lastAction > 14 || d.nearbyNpc < 0 ||
                !std::isfinite(d.x) || !std::isfinite(d.y) || std::abs(d.x) >= 1e10 || std::abs(d.y) >= 1e10)
            {
                return false;
            }
            for (int& trait : d.traits)
            {
                if (!(in >> trait) || trait < 0)
                {
                    return false;
                }
            }
            s.deaths.push_back(d);
            previous = d.id;
        }
        return true;
    }
}
