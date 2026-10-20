#pragma once
#include "GameWorld.h"
#include <locale>
#include <sstream>

namespace Monitoring
{
    struct VillageTotals
    {
        int residents = 0, present = 0, injured = 0, critical = 0, engaged = 0;
        long long gold = 0, goods = 0, insight = 0;
        double health = 0, maxHealth = 0;
        double raidWeight = 0;
        int raiders = 0, recentDeaths = 0, heardRumors = 0;
        std::array<int, Society::JobCount> jobs = {};
        std::array<int, Society::Defending + 1> activities = {};
    };

    inline void Number(std::ostream& out, double value)
    {
        if (std::isfinite(value))
        {
            out << value;
        }
        else
        {
            out << "null";
        }
    }

    inline int RumorReach(const Society::Rumor& r, double now)
    {
        return r.reachMask ? r.reachMask : 1 << r.village;
    }

    inline void String(std::ostream& out, const std::wstring& value)
    {
        // Encode Windows UTF-16 code units as JSON escapes, independent of the process locale.
        static const char hex[] = "0123456789abcdef";
        out << '"';
        for (wchar_t c : value)
        {
            unsigned unit = (unsigned short)c;
            out << "\\u" << hex[unit >> 12] << hex[(unit >> 8) & 15] << hex[(unit >> 4) & 15] << hex[unit & 15];
        }
        out << '"';
    }

    inline int Character(const VillageTotals& v)
    {
        if (!v.residents)
        {
            return 0;
        }
        if (v.raiders * 5 >= v.residents * 2)
        {
            return 1;
        }
        if ((v.jobs[Society::Guardian] + v.jobs[Society::Healer]) * 2 >= v.residents &&
            v.raidWeight / v.residents < .35)
        {
            return 2;
        }
        if (v.jobs[Society::Merchant] * 3 >= v.residents)
        {
            return 3;
        }
        if (v.jobs[Society::Seeker] * 3 >= v.residents)
        {
            return 4;
        }
        return 5;
    }

    inline int Mood(const VillageTotals& v, const Society::Village& market)
    {
        if (!v.residents)
        {
            return 0;
        }
        if (v.engaged || v.recentDeaths || v.critical * 4 >= v.residents)
        {
            return 1;
        }
        if (market.stock < 10 || market.treasury < 20)
        {
            return 2;
        }
        if (v.raiders * 4 >= v.residents || v.heardRumors)
        {
            return 3;
        }
        return 4;
    }

    template <size_t N> void Counts(std::ostream& out, const std::array<int, N>& values)
    {
        out << '[';
        for (size_t i = 0; i < N; ++i)
        {
            if (i)
            {
                out << ',';
            }
            out << values[i];
        }
        out << ']';
    }

    inline int AliveEnemies(const LevelOne::Session& session)
    {
        return (int)std::count_if(
            session.enemies.begin(),
            session.enemies.end(),
            [](const LevelOne::Enemy& enemy)
            {
                return enemy.health > 0;
            }
        );
    }

    // Model text is escaped; the wire format remains ASCII JSON with locale-independent numbers.
    inline std::string Snapshot(
        const Game::World& w,
        unsigned long long session,
        unsigned long long sequence,
        unsigned long long timestamp,
        bool paused,
        bool stopped = false
    )
    {
        std::array<VillageTotals, Society::VillageCount> totals;
        size_t alive = 0;
        for (const auto& n : w.npcs)
        {
            if (!n.life.alive)
            {
                continue;
            }
            ++alive;
            auto& v = totals[n.life.village];
            ++v.residents;
            v.raidWeight += n.life.raidWeight;
            v.raiders += n.life.job == Society::Raider || n.life.raidWeight > .6f;
            v.health += n.life.health;
            v.maxHealth += n.life.maxHealth;
            v.gold += n.life.gold;
            v.goods += n.life.goods;
            v.insight += n.life.insight;
            v.injured += n.life.health < n.life.maxHealth;
            v.critical += n.life.health < n.life.maxHealth * .35f;
            v.engaged += n.life.target != 0;
            ++v.jobs[n.life.job];
            ++v.activities[n.life.activity];
            int present = Game::VillageAt(n.p);
            if (present >= 0)
            {
                ++totals[present].present;
            }
        }
        for (const auto& r : w.society.rumors)
        {
            double age = w.society.elapsed - r.time;
            if (age <= 120)
            {
                ++totals[r.village].recentDeaths;
            }
            if (age <= 300)
            {
                int reach = RumorReach(r, w.society.elapsed);
                for (int i = 0; i < Society::VillageCount; ++i)
                {
                    if (reach & (1 << i))
                    {
                        ++totals[i].heardRumors;
                    }
                }
            }
        }
        const LevelOne::Session* combat = w.storyBattle.active ? &w.storyBattle
                                          : w.levelOne.active  ? &w.levelOne
                                                               : nullptr;
        int mobs = combat ? AliveEnemies(*combat) : 0;
        int spawning = 0, bosses = 0;
        if (combat)
        {
            for (const auto& enemy : combat->enemies)
            {
                if (enemy.health > 0)
                {
                    spawning += enemy.spawnTime > 0;
                    bosses += enemy.kind == LevelOne::Boss;
                }
            }
        }
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(12) << std::boolalpha;
        out << "{\"type\":\"world.snapshot\",\"schemaVersion\":1,\"session\":\"" << session
            << "\",\"sequence\":" << sequence << ",\"timestampMs\":" << timestamp << ",\"paused\":" << paused
            << ",\"stopped\":" << stopped << ",\"worldTime\":";
        Number(out, w.time);
        out << ",\"simulationTime\":";
        Number(out, w.society.elapsed);
        out << ",\"tick\":" << w.society.ticks << ",\"counts\":{\"npcAlive\":" << alive
            << ",\"npcDead\":" << w.npcs.size() - alive << ",\"npcTotal\":" << w.npcs.size() << ",\"mobs\":" << mobs
            << ",\"spawning\":" << spawning << ",\"bosses\":" << bosses
            << ",\"wildlifeEntities\":0},\"player\":{\"health\":";
        Number(out, combat ? combat->health : w.player.health);
        out << ",\"maxHealth\":";
        Number(out, combat ? LevelOne::MaxHealth(*combat) : 100);
        out << ",\"raidGraceSeconds\":";
        Number(out, w.society.playerRaidGrace);
        out << ",\"hitRecoverySeconds\":";
        Number(out, w.society.playerHitRecovery);
        out << ",\"gold\":" << w.society.gold << ",\"deaths\":" << w.deaths << ",\"x\":";
        Number(out, combat ? combat->p.x : w.player.p.x);
        out << ",\"y\":";
        Number(out, combat ? combat->p.y : w.player.p.y);
        out << "},\"combat\":{\"active\":" << (combat != nullptr) << ",\"story\":" << w.storyBattle.active
            << ",\"phase\":" << (combat ? combat->phase : -1) << ",\"encounter\":" << (combat ? combat->encounter : 0)
            << ",\"storedFieldAlive\":" << AliveEnemies(w.levelOne)
            << ",\"storedStoryAlive\":" << AliveEnemies(w.storyBattle)
            << "},\"statistics\":{\"trades\":" << w.society.trades << ",\"raidHits\":" << w.society.raids
            << ",\"npcDeaths\":" << w.society.deaths << "},\"villages\":[";
        for (int i = 0; i < Society::VillageCount; ++i)
        {
            if (i)
            {
                out << ',';
            }
            const auto& v = totals[i];
            auto p = Game::VillageCenter(i);
            out << "{\"id\":" << i << ",\"name\":";
            String(out, Society::VillageName(i));
            out << ",\"x\":" << p.x << ",\"y\":" << p.y << ",\"treasury\":" << w.society.villages[i].treasury
                << ",\"radius\":" << Society::VillageRadius << ",\"housingCapacity\":" << Society::HousingCapacity
                << ",\"stock\":" << w.society.villages[i].stock << ",\"residents\":" << v.residents
                << ",\"present\":" << v.present << ",\"injured\":" << v.injured << ",\"critical\":" << v.critical
                << ",\"engaged\":" << v.engaged << ",\"gold\":" << v.gold << ",\"goods\":" << v.goods
                << ",\"insight\":" << v.insight << ",\"health\":";
            Number(out, v.health);
            out << ",\"maxHealth\":";
            Number(out, v.maxHealth);
            out << ",\"jobs\":";
            Counts(out, v.jobs);
            out << ",\"activities\":";
            Counts(out, v.activities);
            out << ",\"character\":" << Character(v) << ",\"mood\":" << Mood(v, w.society.villages[i])
                << ",\"raiders\":" << v.raiders << ",\"recentDeaths\":" << v.recentDeaths
                << ",\"heardRumors\":" << v.heardRumors << ",\"averageRaidWeight\":";
            Number(out, v.residents ? v.raidWeight / v.residents : 0);
            out << '}';
        }
        out << "],\"npcs\":[";
        for (size_t i = 0; i < w.npcs.size(); ++i)
        {
            if (i)
            {
                out << ',';
            }
            const auto& n = w.npcs[i];
            const auto& l = n.life;
            out << "{\"id\":" << n.id << ",\"alive\":" << l.alive << ",\"name\":";
            String(out, n.Name());
            out << ",\"villageName\":";
            String(out, Society::VillageName(l.village));
            out << ",\"job\":" << l.job << ",\"goal\":" << l.goal << ",\"activity\":" << l.activity
                << ",\"village\":" << l.village << ",\"presentVillage\":" << Game::VillageAt(n.p)
                << ",\"destination\":" << l.destination << ",\"health\":";
            Number(out, l.health);
            out << ",\"maxHealth\":";
            Number(out, l.maxHealth);
            out << ",\"gold\":" << l.gold << ",\"goods\":" << l.goods << ",\"insight\":" << l.insight
                << ",\"raidWeight\":";
            Number(out, l.raidWeight);
            out << ",\"houseLot\":" << l.houseLot << ",\"homeX\":" << n.home.x << ",\"homeY\":" << n.home.y
                << ",\"movingTo\":" << l.movingTo << ",\"carriedRumor\":" << l.carriedRumor
                << ",\"target\":" << l.target << ",\"ability\":" << n.ability << ",\"power\":" << n.power
                << ",\"relationship\":" << n.relationship << ",\"origin\":" << n.origin
                << ",\"originDeathId\":" << n.originDeathId << ",\"parentId\":" << l.parentId
                << ",\"successorId\":" << l.successorId << ",\"x\":";
            Number(out, n.p.x);
            out << ",\"y\":";
            Number(out, n.p.y);
            out << '}';
        }
        out << "],\"events\":[";
        bool first = true;
        for (const auto& e : w.society.events)
        {
            if (!first)
            {
                out << ',';
            }
            first = false;
            out << "{\"time\":";
            Number(out, e.time);
            out << ",\"kind\":" << e.kind << ",\"actor\":" << e.actor << ",\"other\":" << e.other
                << ",\"amount\":" << e.amount << '}';
        }
        out << "],\"rumors\":[";
        first = true;
        for (const auto& r : w.society.rumors)
        {
            if (!first)
            {
                out << ',';
            }
            first = false;
            double age = std::max(0.0, w.society.elapsed - r.time);
            out << "{\"id\":" << r.id << ",\"kind\":\"murder\",\"actor\":" << r.actor << ",\"victim\":" << r.victim
                << ",\"village\":" << r.village << ",\"witnesses\":" << r.witnesses
                << ",\"reachMask\":" << RumorReach(r, w.society.elapsed) << ",\"active\":" << (age <= 300)
                << ",\"time\":";
            Number(out, r.time);
            out << ",\"age\":";
            Number(out, age);
            out << ",\"x\":";
            Number(out, r.x);
            out << ",\"y\":";
            Number(out, r.y);
            out << '}';
        }
        out << "],\"letters\":[";
        first = true;
        for (const auto& letter : w.society.letters)
        {
            if (!first)
            {
                out << ',';
            }
            first = false;
            out << "{\"from\":" << letter.from << ",\"to\":" << letter.to << ",\"origin\":" << letter.origin
                << ",\"rumorId\":" << letter.rumorId << ",\"arrival\":" << letter.arrival
                << ",\"remaining\":" << std::max(0.0, letter.arrival - w.society.elapsed) << '}';
        }
        out << "]}";
        return out.str();
    }
}
