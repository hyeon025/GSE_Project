#pragma once
#include <algorithm>
#include <array>
#include <deque>
#include <cmath>
#include <istream>
#include <ostream>
#include <string>
#include "NpcNavigationState.h"

namespace Society
{
    constexpr int VillageCount = 7;
    constexpr int ResidentsPerVillage = 24;
    constexpr int HousingCapacity = 60;
    constexpr double VillageRadius = 850;
    constexpr double WalkingSpeed = 85;
    constexpr double VillageSpacing = WalkingSpeed * 20;
    constexpr double WorkInterval = 5;

    inline const wchar_t* VillageName(int village)
    {
        static const wchar_t* names[VillageCount] = {
            L"\uc7bf\ube5b \ub9c8\uc744",
            L"\uc885\ud0d1 \ub9c8\uc744",
            L"\uac15\ubcc0 \ub9c8\uc744",
            L"\uc548\uac1c\uc232 \ub9c8\uc744",
            L"\uac80\uc740\ub098\ub8e8 \ub9c8\uc744",
            L"\ubc31\uc11d \ub9c8\uc744",
            L"\uc0c8\ubcbd \ub9c8\uc744"
        };
        return village >= 0 && village < VillageCount ? names[village] : L"\uc18c\uc18d \uc5c6\uc74c";
    }

    inline std::wstring PersonalName(int id)
    {
        static const wchar_t* given[] = {
            L"\uc5d0\ub4dc\ub9ad",
            L"\ubbf8\ub77c",
            L"\ub85c\uc6ec",
            L"\uc5d8\ub77c",
            L"\uac00\ub80c",
            L"\ub9ac\uc624",
            L"\uc138\ub77c",
            L"\uc624\ub9b0",
            L"\ub9c8\ub974\ud0c0",
            L"\ub8e8\uce74",
            L"\uc774\ub9ac\uc2a4",
            L"\ud14c\uc624",
            L"\ub808\ub098",
            L"\uce74\uc77c",
            L"\ub2c8\uc544",
            L"\ubc14\ub80c"
        };
        static const wchar_t* family[] = {
            L"\uc560\uc26c\ud3ec\ub4dc",
            L"\uadf8\ub808\uc774",
            L"\ub9ac\ubc84\uc2a4",
            L"\ube14\ub799\uc6b0\ub4dc",
            L"\ubca8\ubaac\ud2b8",
            L"\ud06c\ub85c\ud504\ud2b8",
            L"\uc2a4\ud1a4",
            L"\ud5e4\uc77c",
            L"\ub178\uc2a4",
            L"\ub9ac\ub4dc",
            L"\uc6cc\ub4dc",
            L"\ubca0\uc77c",
            L"\ud558\ud2b8",
            L"\ub85c\ud06c",
            L"\ub378",
            L"\ud398\uc778"
        };
        if (id <= 0)
        {
            return L"\uc774\ub984 \uc5c6\ub294 \uc790";
        }
        // Identity comes from the persisted NPC ID, never job, home or array order.
        unsigned index = (unsigned)(id - 1);
        std::wstring name = std::wstring(given[index % 16]) + L" " + family[index / 16 % 16];
        if (index >= 256)
        {
            name += L" " + std::to_wstring(index / 256 + 1);
        }
        return name;
    }

    enum Job
    {
        Hunter,
        Merchant,
        Raider,
        Seeker,
        Guardian,
        Healer,
        JobCount
    };

    enum Goal
    {
        Vitality,
        Wealth,
        Insight
    };

    enum Activity
    {
        Recovering,
        Hunting,
        Trading,
        Travelling,
        Raiding,
        Studying,
        Patrolling,
        Healing,
        Defending,
        Resting,
        Wandering,
        ActivityCount
    };

    enum EventKind
    {
        HuntCompleted,
        GoodsSold,
        GoodsBought,
        Assault,
        Killed,
        SuccessorBorn,
        Enlightened,
        AidGiven,
        LetterDelivered,
        MigrationStarted,
        MigrationCompleted
    };

    struct News
    {
        double time = -1;
        float danger = 0;
        int stock = 0, residents = 0;
    };

    inline void WriteNews(std::ostream& out, const News& n)
    {
        out << n.time << ' ' << n.danger << ' ' << n.stock << ' ' << n.residents << ' ';
    }

    inline bool ReadNews(std::istream& in, News& n)
    {
        return (in >> n.time >> n.danger >> n.stock >> n.residents) && std::isfinite(n.time) && n.time >= -1 &&
               n.danger >= 0 && n.danger <= 1 && n.stock >= 0 && n.stock <= 1000 && n.residents >= 0 &&
               n.residents <= 200000;
    }

    struct Life
    {
        bool initialized = false, alive = true;
        int job = Hunter, goal = Wealth, activity = Recovering, village = 0, destination = 0;
        int gold = 20, goods = 0, insight = 0, target = 0, parentId = 0, successorId = 0;
        float health = 100, maxHealth = 100, raidWeight = 0;
        double cooldown = 0, work = 0, birthAt = 0;
        // Encounter timers are session-local, not part of the persistent life record.
        double playerChase = 0, playerRetry = 0, playerProvoked = 0;
        Navigation navigation;
        Routine routine;
        int houseLot = -1, movingTo = -1, carriedRumor = 0;
        double migrationCooldown = 0;
        std::array<News, VillageCount> known;
    };

    struct Village
    {
        int treasury = 600, stock = 80;
        double production = 0;
        std::array<News, VillageCount> news;
    };

    struct Event
    {
        double time = 0;
        int kind = 0, actor = 0, other = 0, amount = 0;
    };

    struct Rumor
    {
        int id = 0, actor = 0, victim = 0, village = 0, witnesses = 0;
        double time = 0, x = 0, y = 0;
        int reachMask = 0;
    };

    struct Letter
    {
        int from = 0, to = 0, origin = 0, rumorId = 0;
        double arrival = 0;
        News news;
    };

    struct State
    {
        bool initialized = false;
        int gold = 30;
        double elapsed = 0, accumulator = 0, playerCooldown = 0, logAt = 10;
        double playerRaidGrace = 60, playerHitRecovery = 0;
        unsigned long long ticks = 0, trades = 0, raids = 0, deaths = 0;
        int settledVillages = 0;
        std::array<Village, VillageCount> villages;
        std::deque<Event> events;
        int nextRumorId = 1;
        std::deque<Rumor> rumors;
        int layoutVersion = 0;
        std::array<int, VillageCount> seeded = {};
        double nextNews = 0;
        std::deque<Letter> letters;
        int navigationBudget = 4; // Bounded path searches per simulation tick.
    };

    inline void WriteLife(std::ostream& out, const Life& n, bool extended = false)
    {
        out << n.initialized << ' ' << n.alive << ' ' << n.job << ' ' << n.goal << ' ' << n.activity << ' ' << n.village
            << ' ' << n.destination << ' ' << n.gold << ' ' << n.goods << ' ' << n.insight << ' ' << n.target << ' '
            << n.parentId << ' ' << n.successorId << ' ' << n.health << ' ' << n.maxHealth << ' ' << n.raidWeight << ' '
            << n.cooldown << ' ' << n.work << ' ' << n.birthAt << '\n';
        if (extended)
        {
            out << n.houseLot << ' ' << n.movingTo << ' ' << n.carriedRumor << ' ' << n.migrationCooldown << ' ';
            for (const auto& news : n.known)
            {
                WriteNews(out, news);
            }
            out << '\n';
        }
    }

    inline bool ReadLife(std::istream& in, Life& n, int count, bool extended = false)
    {
        n.navigation = Navigation();
        n.routine = Routine();
        n.playerChase = n.playerRetry = n.playerProvoked = 0;
        in >> n.initialized >> n.alive >> n.job >> n.goal >> n.activity >> n.village >> n.destination >> n.gold >>
            n.goods >> n.insight >> n.target >> n.parentId >> n.successorId >> n.health >> n.maxHealth >>
            n.raidWeight >> n.cooldown >> n.work >> n.birthAt;
        bool valid = in && n.job >= 0 && n.job < JobCount && n.goal >= Vitality && n.goal <= Insight &&
                     n.activity >= Recovering && n.activity < ActivityCount && n.village >= 0 &&
                     n.village < VillageCount && n.destination >= 0 && n.destination < VillageCount && n.gold >= 0 &&
                     n.gold <= 1000000 && n.goods >= 0 && n.goods <= 1000 && n.insight >= 0 && n.insight <= 10000 &&
                     n.target >= -1 && n.target <= count && n.parentId >= 0 && n.parentId <= count &&
                     n.successorId >= 0 && n.successorId <= count && n.maxHealth >= 100 && n.maxHealth <= 200 &&
                     n.health >= 0 && n.health <= n.maxHealth && (n.alive == (n.health > 0)) && n.raidWeight >= 0 &&
                     n.raidWeight <= 1 && n.cooldown >= 0 && n.cooldown <= 60 && std::isfinite(n.work) && n.work >= 0 &&
                     std::isfinite(n.birthAt) && n.birthAt >= 0;
        if (!valid)
        {
            return false;
        }
        // Legacy recovery accumulated work indefinitely; keep only usable task progress.
        n.work = n.activity == Recovering ? 0 : std::min(n.work, WorkInterval);
        if (extended)
        {
            in >> n.houseLot >> n.movingTo >> n.carriedRumor >> n.migrationCooldown;
            if (!in || n.houseLot < -1 || n.houseLot >= HousingCapacity || n.movingTo < -1 ||
                n.movingTo >= VillageCount || n.carriedRumor < 0 || !std::isfinite(n.migrationCooldown) ||
                n.migrationCooldown < 0)
            {
                return false;
            }
            for (auto& news : n.known)
            {
                if (!ReadNews(in, news))
                {
                    return false;
                }
            }
        }
        return true;
    }

    inline void Write(std::ostream& out, const State& s)
    {
        out << "SOCIETY 4 " << s.initialized << ' ' << s.gold << ' ' << s.elapsed << ' ' << s.accumulator << ' '
            << s.playerCooldown << ' ' << s.ticks << ' ' << s.trades << ' ' << s.raids << ' ' << s.deaths << '\n';
        out << VillageCount << ' ' << s.settledVillages << '\n';
        for (const auto& v : s.villages)
        {
            out << v.treasury << ' ' << v.stock << ' ' << v.production << '\n';
        }
        out << s.events.size() << '\n';
        for (const auto& e : s.events)
        {
            out << e.time << ' ' << e.kind << ' ' << e.actor << ' ' << e.other << ' ' << e.amount << '\n';
        }
        out << s.nextRumorId << ' ' << s.rumors.size() << '\n';
        for (const auto& r : s.rumors)
        {
            out << r.id << ' ' << r.time << ' ' << r.actor << ' ' << r.victim << ' ' << r.village << ' ' << r.witnesses
                << ' ' << r.x << ' ' << r.y << '\n';
            out << r.reachMask << '\n';
        }
        out << s.layoutVersion << ' ' << s.nextNews << '\n';
        for (int seeded : s.seeded)
        {
            out << seeded << ' ';
        }
        out << '\n';
        for (const auto& v : s.villages)
        {
            for (const auto& news : v.news)
            {
                WriteNews(out, news);
            }
        }
        out << '\n' << s.letters.size() << '\n';
        for (const auto& letter : s.letters)
        {
            out << letter.from << ' ' << letter.to << ' ' << letter.origin << ' ' << letter.rumorId << ' '
                << letter.arrival << ' ';
            WriteNews(out, letter.news);
            out << '\n';
        }
    }

    inline bool Read(std::istream& in, State& s, int count)
    {
        s.playerRaidGrace = 60;
        s.playerHitRecovery = 0;
        std::string magic;
        int version;
        in >> magic >> version >> s.initialized >> s.gold >> s.elapsed >> s.accumulator >> s.playerCooldown >>
            s.ticks >> s.trades >> s.raids >> s.deaths;
        if (!in || magic != "SOCIETY" || version < 1 || version > 4 || s.gold < 0 || s.gold > 1000000 ||
            !std::isfinite(s.elapsed) || s.elapsed < 0 || !(s.accumulator >= 0 && s.accumulator < .101) ||
            !(s.playerCooldown >= 0 && s.playerCooldown <= 60))
        {
            return false;
        }
        int storedVillages = 3;
        s.settledVillages = s.initialized ? 3 : 0;
        if (version >= 3)
        {
            in >> storedVillages >> s.settledVillages;
            if (!in || storedVillages < 1 || storedVillages > VillageCount || s.settledVillages < 0 ||
                s.settledVillages > storedVillages || (!s.initialized && s.settledVillages != 0))
            {
                return false;
            }
        }
        s.villages = {};
        for (int i = 0; i < storedVillages; ++i)
        {
            auto& v = s.villages[i];
            in >> v.treasury >> v.stock >> v.production;
            if (!in || v.treasury < 0 || v.treasury > 1000000 || v.stock < 0 || v.stock > 1000 ||
                !(v.production >= 0 && v.production < 20))
            {
                return false;
            }
        }
        size_t size;
        if (!(in >> size) || size > 128)
        {
            return false;
        }
        s.events.clear();
        for (size_t i = 0; i < size; ++i)
        {
            Event e;
            in >> e.time >> e.kind >> e.actor >> e.other >> e.amount;
            if (!in || !(e.time >= 0 && e.time <= s.elapsed) || e.kind < HuntCompleted || e.kind > MigrationCompleted ||
                e.actor < 0 || e.actor > count || e.other < 0 || e.other > count || e.amount < 0 || e.amount > 1000000)
            {
                return false;
            }
            s.events.push_back(e);
        }
        s.rumors.clear();
        s.nextRumorId = 1;
        if (version >= 2)
        {
            if (!(in >> s.nextRumorId >> size) || s.nextRumorId < 1 || size > 64)
            {
                return false;
            }
            int previous = 0;
            for (size_t i = 0; i < size; ++i)
            {
                Rumor r;
                in >> r.id >> r.time >> r.actor >> r.victim >> r.village >> r.witnesses >> r.x >> r.y;
                if (!in || r.id <= previous || r.id >= s.nextRumorId || !(r.time >= 0 && r.time <= s.elapsed) ||
                    r.actor < 0 || r.actor > count || r.victim < 0 || r.victim > count || r.village < 0 ||
                    r.village >= storedVillages || r.witnesses < 0 || r.witnesses > count || !std::isfinite(r.x) ||
                    !std::isfinite(r.y) || std::abs(r.x) >= 1e10 || std::abs(r.y) >= 1e10)
                {
                    return false;
                }
                previous = r.id;
                r.reachMask = 1 << r.village;
                if (version >= 4 && (!(in >> r.reachMask) || r.reachMask < 1 || r.reachMask >= (1 << VillageCount)))
                {
                    return false;
                }
                s.rumors.push_back(r);
            }
        }
        s.layoutVersion = 0;
        s.seeded.fill(0);
        for (int v = 0; v < s.settledVillages; ++v)
        {
            s.seeded[v] = 8;
        }
        s.letters.clear();
        if (version >= 4)
        {
            if (!(in >> s.layoutVersion >> s.nextNews) || s.layoutVersion < 0 || s.layoutVersion > 2 ||
                !std::isfinite(s.nextNews) || s.nextNews < 0)
            {
                return false;
            }
            for (int& seeded : s.seeded)
            {
                if (!(in >> seeded) || seeded < 0 || seeded > HousingCapacity)
                {
                    return false;
                }
            }
            for (auto& v : s.villages)
            {
                for (auto& news : v.news)
                {
                    if (!ReadNews(in, news))
                    {
                        return false;
                    }
                }
            }
            if (!(in >> size) || size > 1024)
            {
                return false;
            }
            for (size_t i = 0; i < size; ++i)
            {
                Letter letter;
                in >> letter.from >> letter.to >> letter.origin >> letter.rumorId >> letter.arrival;
                if (!in || letter.from < 0 || letter.from >= VillageCount || letter.to < 0 ||
                    letter.to >= VillageCount || letter.origin < 0 || letter.origin >= VillageCount ||
                    letter.rumorId < 0 || !std::isfinite(letter.arrival) || letter.arrival < 0 ||
                    !ReadNews(in, letter.news))
                {
                    return false;
                }
                s.letters.push_back(letter);
            }
        }
        s.logAt = s.elapsed + 10;
        return true;
    }
}
