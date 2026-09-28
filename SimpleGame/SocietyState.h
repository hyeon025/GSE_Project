#pragma once
#include <array>
#include <deque>
#include <cmath>
#include <istream>
#include <ostream>
#include <string>

namespace Society
{
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
        Defending
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
        AidGiven
    };

    struct Life
    {
        bool initialized = false, alive = true;
        int job = Hunter, goal = Wealth, activity = Recovering, village = 0, destination = 0;
        int gold = 20, goods = 0, insight = 0, target = 0, parentId = 0, successorId = 0;
        float health = 100, maxHealth = 100, raidWeight = 0;
        double cooldown = 0, work = 0, birthAt = 0;
    };

    struct Village
    {
        int treasury = 600, stock = 80;
        double production = 0;
    };

    struct Event
    {
        double time = 0;
        int kind = 0, actor = 0, other = 0, amount = 0;
    };

    struct State
    {
        bool initialized = false;
        int gold = 30;
        double elapsed = 0, accumulator = 0, playerCooldown = 0, logAt = 10;
        unsigned long long ticks = 0, trades = 0, raids = 0, deaths = 0;
        std::array<Village, 3> villages;
        std::deque<Event> events;
    };

    inline void WriteLife(std::ostream& out, const Life& n)
    {
        out << n.initialized << ' ' << n.alive << ' ' << n.job << ' ' << n.goal << ' ' << n.activity << ' ' << n.village
            << ' ' << n.destination << ' ' << n.gold << ' ' << n.goods << ' ' << n.insight << ' ' << n.target << ' '
            << n.parentId << ' ' << n.successorId << ' ' << n.health << ' ' << n.maxHealth << ' ' << n.raidWeight << ' '
            << n.cooldown << ' ' << n.work << ' ' << n.birthAt << '\n';
    }

    inline bool ReadLife(std::istream& in, Life& n, int count)
    {
        in >> n.initialized >> n.alive >> n.job >> n.goal >> n.activity >> n.village >> n.destination >> n.gold >>
            n.goods >> n.insight >> n.target >> n.parentId >> n.successorId >> n.health >> n.maxHealth >>
            n.raidWeight >> n.cooldown >> n.work >> n.birthAt;
        return in && n.job >= 0 && n.job < JobCount && n.goal >= Vitality && n.goal <= Insight &&
               n.activity >= Recovering && n.activity <= Defending && n.village >= 0 && n.village < 3 &&
               n.destination >= 0 && n.destination < 3 && n.gold >= 0 && n.gold <= 1000000 && n.goods >= 0 &&
               n.goods <= 1000 && n.insight >= 0 && n.insight <= 10000 && n.target >= -1 && n.target <= count &&
               n.parentId >= 0 && n.parentId <= count && n.successorId >= 0 && n.successorId <= count &&
               n.maxHealth >= 100 && n.maxHealth <= 200 && n.health >= 0 && n.health <= n.maxHealth &&
               (n.alive == (n.health > 0)) && n.raidWeight >= 0 && n.raidWeight <= 1 && n.cooldown >= 0 &&
               n.cooldown <= 60 && n.work >= 0 && n.work <= 60 && std::isfinite(n.birthAt) && n.birthAt >= 0;
    }

    inline void Write(std::ostream& out, const State& s)
    {
        out << "SOCIETY 1 " << s.initialized << ' ' << s.gold << ' ' << s.elapsed << ' ' << s.accumulator << ' '
            << s.playerCooldown << ' ' << s.ticks << ' ' << s.trades << ' ' << s.raids << ' ' << s.deaths << '\n';
        for (const auto& v : s.villages)
        {
            out << v.treasury << ' ' << v.stock << ' ' << v.production << '\n';
        }
        out << s.events.size() << '\n';
        for (const auto& e : s.events)
        {
            out << e.time << ' ' << e.kind << ' ' << e.actor << ' ' << e.other << ' ' << e.amount << '\n';
        }
    }

    inline bool Read(std::istream& in, State& s, int count)
    {
        std::string magic;
        int version;
        in >> magic >> version >> s.initialized >> s.gold >> s.elapsed >> s.accumulator >> s.playerCooldown >>
            s.ticks >> s.trades >> s.raids >> s.deaths;
        if (!in || magic != "SOCIETY" || version != 1 || s.gold < 0 || s.gold > 1000000 || !std::isfinite(s.elapsed) ||
            s.elapsed < 0 || !(s.accumulator >= 0 && s.accumulator < .101) ||
            !(s.playerCooldown >= 0 && s.playerCooldown <= 60))
        {
            return false;
        }
        for (auto& v : s.villages)
        {
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
            if (!in || !(e.time >= 0 && e.time <= s.elapsed) || e.kind < HuntCompleted || e.kind > AidGiven ||
                e.actor < 0 || e.actor > count || e.other < 0 || e.other > count || e.amount < 0 || e.amount > 1000000)
            {
                return false;
            }
            s.events.push_back(e);
        }
        s.logAt = s.elapsed + 10;
        return true;
    }
}
