#pragma once
#include "GameWorld.h"
#include <iostream>

namespace Society
{
    inline const wchar_t* JobName(int job)
    {
        static const wchar_t* names[] = {
            L"\uc0ac\ub0e5\uafbc",
            L"\uc0c1\uc778",
            L"\uc57d\ud0c8\uc790",
            L"\uad6c\ub3c4\uc790",
            L"\uc218\ud638\uc790",
            L"\uce58\uc720\uc0ac"
        };
        return names[job];
    }

    inline const wchar_t* ActivityName(int activity)
    {
        static const wchar_t* names[] = {
            L"\ud68c\ubcf5",
            L"\uc0ac\ub0e5",
            L"\uac70\ub798",
            L"\uc774\ub3d9",
            L"\uc57d\ud0c8",
            L"\uba85\uc0c1",
            L"\uc21c\ucc30",
            L"\uce58\ub8cc",
            L"\ubc29\uc5b4"
        };
        return names[activity];
    }

    inline const wchar_t* GoalName(int goal)
    {
        static const wchar_t* names[] = {L"\uccb4\ub825", L"\uc7ac\uc0b0", L"\uae68\ub2ec\uc74c"};
        return names[goal];
    }

    inline void Record(Game::World& w, int kind, int actor, int other = 0, int amount = 0)
    {
        auto& s = w.society;
        if (s.events.size() == 128)
        {
            s.events.pop_front();
        }
        s.events.push_back({s.elapsed, kind, actor, other, amount});
        if (kind == Killed && s.nextRumorId < 2147483647)
        {
            Rumor rumor;
            rumor.id = s.nextRumorId++;
            rumor.time = s.elapsed;
            rumor.actor = actor;
            rumor.victim = other;
            Game::Vec position = other > 0 && other <= (int)w.npcs.size() ? w.npcs[other - 1].p : w.player.p;
            rumor.x = position.x;
            rumor.y = position.y;
            for (int v = 1; v < VillageCount; ++v)
            {
                if (Game::Distance(position, Game::VillageCenter(v)) <
                    Game::Distance(position, Game::VillageCenter(rumor.village)))
                {
                    rumor.village = v;
                }
            }
            for (const auto& n : w.npcs)
            {
                if (n.life.alive && n.id != actor && n.id != other && Game::Distance(n.p, position) < 180)
                {
                    ++rumor.witnesses;
                }
            }
            if (s.rumors.size() == 64)
            {
                s.rumors.pop_front();
            }
            rumor.reachMask = 1 << rumor.village;
            s.rumors.push_back(rumor);
            for (int to = 0; to < VillageCount && s.letters.size() < 1024; ++to)
            {
                if (to == rumor.village)
                {
                    continue;
                }
                Letter letter;
                letter.from = letter.origin = rumor.village;
                letter.to = to;
                letter.rumorId = rumor.id;
                auto fromCenter = Game::VillageCenter(rumor.village), toCenter = Game::VillageCenter(to);
                letter.arrival =
                    s.elapsed + (std::abs(fromCenter.x - toCenter.x) + std::abs(fromCenter.y - toCenter.y)) / 48;
                letter.news = s.villages[rumor.village].news[rumor.village];
                s.letters.push_back(letter);
            }
        }
        w.dirty = true;
    }

    inline int NearestVillage(Game::Vec p)
    {
        int result = 0;
        for (int i = 1; i < VillageCount; ++i)
        {
            if (Game::Distance(p, Game::VillageCenter(i)) < Game::Distance(p, Game::VillageCenter(result)))
            {
                result = i;
            }
        }
        return result;
    }

    inline void InitializeNpc(Game::Npc& n)
    {
        if (n.life.initialized)
        {
            return;
        }
        static const int jobs[] = {Raider, Healer, Hunter, Seeker, Guardian, Merchant, Hunter};
        n.life.job = jobs[n.archetype];
        n.life.village = n.life.destination = NearestVillage(n.home);
        n.home = Game::VillageCenter(n.life.village);
        float aggression = (float)n.echo[0], greed = (float)n.echo[5], mercy = (float)n.echo[1];
        n.life.raidWeight = std::max(0.f, std::min(1.f, (aggression + greed) / (30.f + aggression + greed + mercy)));
        if (n.life.job == Raider)
        {
            n.life.raidWeight = std::max(.8f, n.life.raidWeight);
        }
        n.life.goal = n.life.job == Seeker ? Insight : Wealth;
        n.life.maxHealth = n.ability == Game::Body ? 120.f : 100.f;
        n.life.health = n.life.maxHealth;
        n.life.gold = n.life.job == Merchant ? 65 : 20;
        n.life.goods = n.life.job == Merchant ? 5 : 0;
        n.life.initialized = true;
        if (!Game::Walkable(n.p))
        {
            n.p = n.home;
        }
    }

    inline void Initialize(Game::World& w)
    {
        bool expanded = w.society.layoutVersion == 0;
        if (expanded)
        {
            static const int traits[VillageCount][8] = {
                {6, 2, 5, 5, 0, 3, 4, 1},
                {6, 2, 5, 5, 0, 3, 4, 1},
                {6, 2, 5, 5, 0, 3, 4, 1},
                {6, 2, 5, 3, 3, 3, 4, 1},
                {6, 2, 5, 0, 0, 0, 0, 1},
                {6, 2, 5, 4, 4, 1, 4, 1},
                {6, 2, 5, 3, 1, 3, 4, 1}
            };
            // Upgrade only once; dead residents remain dead and are not replenished on reload.
            for (auto& n : w.npcs)
            {
                InitializeNpc(n);
                if (n.life.alive)
                {
                    n.p = Game::VillageCenter(n.life.village);
                }
                n.life.houseLot = -1;
            }
            for (int village = 0; village < VillageCount; ++village)
            {
                for (int i = w.society.seeded[village]; i < ResidentsPerVillage && w.npcs.size() < 200000; ++i)
                {
                    Game::Npc n;
                    n.id = (int)w.npcs.size() + 1;
                    n.origin = 2;
                    n.archetype = traits[village][i % 8];
                    n.echo[n.archetype] = 24;
                    n.ability = (Game::Ability)(Game::Hash(n.id, 73, 101) % Game::AbilityCount);
                    n.home = Game::VillageCenter(village);
                    n.p = Game::Vec(n.home.x + (i % 4 - 1.5) * 18, n.home.y + (i / 4 ? 18 : -18));
                    InitializeNpc(n);
                    w.npcs.push_back(n);
                    w.society.seeded[village] = i + 1;
                }
                w.society.settledVillages = village + 1;
            }
            w.society.initialized = true;
            w.society.layoutVersion = 1;
            w.dirty = true;
            if (!Game::Walkable(w.player.p))
            {
                w.player.p = Game::VillageCenter(NearestVillage(w.player.p));
            }
            if (!Game::Walkable(w.player.checkpoint))
            {
                w.player.checkpoint = Game::VillageCenter(NearestVillage(w.player.checkpoint));
            }
            std::cout << "[World] villages=" << VillageCount << " settled=" << w.society.settledVillages
                      << " residentsPerVillage=" << ResidentsPerVillage << " npcRecords=" << w.npcs.size() << '\n';
        }
        for (auto& n : w.npcs)
        {
            InitializeNpc(n);
            if (n.life.houseLot < 0)
            {
                std::array<bool, HousingCapacity> occupied = {};
                for (const auto& other : w.npcs)
                {
                    if (other.life.alive && other.life.village == n.life.village && other.life.houseLot >= 0)
                    {
                        occupied[other.life.houseLot] = true;
                    }
                }
                for (int lot = 0; lot < HousingCapacity; ++lot)
                {
                    if (!occupied[lot])
                    {
                        n.life.houseLot = lot;
                        break;
                    }
                }
                if (n.life.houseLot >= 0)
                {
                    n.home = Game::HouseEntrance(n.life.village, n.life.houseLot);
                    if (expanded || !Game::Walkable(n.p))
                    {
                        n.p = n.home;
                    }
                }
            }
            if (expanded && n.life.alive && !Game::Walkable(n.p))
            {
                n.p = n.home;
            }
        }
    }

    inline Game::Npc* Find(Game::World& w, int id)
    {
        return id > 0 && id <= (int)w.npcs.size() ? &w.npcs[id - 1] : nullptr;
    }

    inline void Die(Game::World& w, Game::Npc& n, int killer)
    {
        if (!n.life.alive)
        {
            return;
        }
        // Keep the unique story child available until its quest is complete.
        if (n.id == w.story.childId && !w.story.childComplete)
        {
            n.life.health = n.life.maxHealth;
            n.life.target = 0;
            n.p = n.home;
            return;
        }
        n.life.alive = false;
        n.life.health = 0;
        n.life.target = 0;
        n.life.birthAt = w.society.elapsed + 20;
        ++w.society.deaths;
        Record(w, Killed, killer, n.id, n.life.gold);
        if (killer == 0)
        {
            int loot = std::min(n.life.gold, 1000000 - w.society.gold);
            w.society.gold += loot;
            n.life.gold -= loot;
        }
        else if (auto* victor = Find(w, killer))
        {
            int loot = std::min(n.life.gold, 1000000 - victor->life.gold);
            victor->life.gold += loot;
            n.life.gold -= loot;
            int goods = std::min(n.life.goods, 1000 - victor->life.goods);
            victor->life.goods += goods;
            n.life.goods -= goods;
            victor->power = std::min(100, victor->power + 2);
        }
    }

    inline void Births(Game::World& w)
    {
        const size_t count = w.npcs.size();
        for (size_t i = 0; i < count; ++i)
        {
            auto& old = w.npcs[i];
            if (old.life.alive || old.life.successorId || old.life.birthAt > w.society.elapsed ||
                w.story.ending == Story::Destroy || w.npcs.size() >= 200000)
            {
                continue;
            }
            Game::Npc n;
            n.id = (int)w.npcs.size() + 1;
            n.origin = 1;
            n.echo = old.echo;
            n.freeWill = w.story.ending == Story::Freedom;
            if (n.freeWill)
            {
                n.echo.fill(0);
                n.echo[Game::Hash(n.id, 17, 9) % 7] = 12;
            }
            n.archetype = (int)(std::max_element(n.echo.begin(), n.echo.end()) - n.echo.begin());
            n.ability = (Game::Ability)(Game::Hash(n.id, 73, 101) % Game::AbilityCount);
            n.p = n.home = Game::VillageCenter((old.life.village + 1) % VillageCount);
            n.life.parentId = old.id;
            InitializeNpc(n);
            auto& treasury = w.society.villages[n.life.village].treasury;
            n.life.gold = std::min(20, treasury);
            n.life.goods = 0;
            treasury -= n.life.gold;
            old.life.successorId = n.id;
            Record(w, SuccessorBorn, n.id, old.id);
            w.npcs.push_back(n);
        }
    }

    inline int Price(const Village& village)
    {
        return village.stock < 25 ? 5 : village.stock < 60 ? 4 : 3;
    }

    inline bool Secondary(const Game::Npc& n, Game::Ability ability)
    {
        return n.life.insight >= 10 && Game::Adjacent(n.ability, ability);
    }

    inline void Sell(Game::World& w, Game::Npc& n, int village)
    {
        auto& v = w.society.villages[village];
        int price = Price(v);
        int quantity = std::min({n.life.goods, v.treasury / price, 1000 - v.stock, (1000000 - n.life.gold) / price});
        if (quantity <= 0)
        {
            return;
        }
        v.treasury -= quantity * price;
        v.stock += quantity;
        n.life.gold += quantity * price;
        n.life.goods -= quantity;
        ++w.society.trades;
        n.echo[5] = std::min(1000000, n.echo[5] + 1);
        Record(w, GoodsSold, n.id, 0, quantity);
    }

    inline bool ClearSegment(Game::Vec a, Game::Vec b)
    {
        int steps = std::max(1, (int)std::ceil(Game::Distance(a, b) / 8));
        for (int i = 1; i <= steps; ++i)
        {
            double t = (double)i / steps;
            if (!Game::Walkable(Game::Vec(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t)))
            {
                return false;
            }
        }
        return true;
    }

    inline bool Move(Game::Npc& n, Game::Vec goal, double dt)
    {
        double distance = Game::Distance(n.p, goal);
        if (distance < 12)
        {
            n.life.routeVillage = -1;
            return true;
        }
        int destinationVillage = Game::VillageAt(goal);
        if (!n.life.target && destinationVillage >= 0)
        {
            if (n.life.routeTarget != destinationVillage)
            {
                n.life.routeVillage = -1;
                n.life.routeTarget = destinationVillage;
            }
            if (n.life.routeVillage >= 0 && Game::Distance(n.p, Game::VillageCenter(n.life.routeVillage)) < 12)
            {
                n.life.routeVillage = -1;
            }
            int source = NearestVillage(n.p);
            if (n.life.routeVillage < 0 && source != destinationVillage)
            {
                if (Game::Distance(n.p, Game::VillageCenter(source)) > 20)
                {
                    n.life.routeVillage = source;
                }
                else
                {
                    std::array<int, VillageCount> parent;
                    parent.fill(-1);
                    std::queue<int> pending;
                    pending.push(source);
                    parent[source] = source;
                    while (!pending.empty() && parent[destinationVillage] < 0)
                    {
                        int from = pending.front();
                        pending.pop();
                        for (int to = 0; to < VillageCount; ++to)
                        {
                            if (parent[to] < 0 && Game::Distance(Game::VillageCenter(from), Game::VillageCenter(to)) <=
                                                      VillageSpacing * 1.01)
                            {
                                parent[to] = from;
                                pending.push(to);
                            }
                        }
                    }
                    int hop = destinationVillage;
                    while (parent[hop] >= 0 && parent[hop] != source && parent[hop] != hop)
                    {
                        hop = parent[hop];
                    }
                    n.life.routeVillage = hop;
                }
            }
            if (n.life.routeVillage >= 0)
            {
                goal = Game::VillageCenter(n.life.routeVillage);
            }
            distance = Game::Distance(n.p, goal);
            if (distance < .001)
            {
                return false;
            }
        }
        // Residential travel first joins the street grid, then follows its junctions.
        // Keep combat pursuit on the existing steering path.
        n.life.pathBudget -= dt;
        if (n.life.pathBudget <= 0 || goal.x != n.life.pathX || goal.y != n.life.pathY)
        {
            n.life.pathX = goal.x;
            n.life.pathY = goal.y;
            n.life.pathClear = ClearSegment(n.p, goal);
            n.life.pathBudget = .5;
        }
        if (!n.life.target && !n.life.pathClear)
        {
            if (n.life.streetActive && Game::Distance(n.p, {n.life.streetX, n.life.streetY}) < 10)
            {
                n.life.streetActive = false;
            }
            if (n.life.streetActive)
            {
                goal = Game::Vec(n.life.streetX, n.life.streetY);
            }
            else
            {
                int village = Game::VillageAt(n.p);
                if (village >= 0)
                {
                    auto center = Game::VillageCenter(village);
                    Game::Vec junction(
                        center.x + std::round((n.p.x - center.x) / 112) * 112,
                        center.y + std::round((n.p.y - center.y) / 112) * 112
                    );
                    if (Game::Distance(n.p, junction) > 10 && ClearSegment(n.p, junction))
                    {
                        goal = junction;
                    }
                    else
                    {
                        Game::Vec destination = Game::VillageAt(goal) == village ? goal : center;
                        Game::Vec next(destination.x, junction.y);
                        if (Game::Distance(n.p, next) < 10)
                        {
                            next = Game::Vec(junction.x, destination.y);
                        }
                        if (ClearSegment(n.p, next))
                        {
                            goal = next;
                        }
                        else
                        {
                            goal = Game::Vec(junction.x, destination.y);
                        }
                    }
                }
                n.life.streetX = goal.x;
                n.life.streetY = goal.y;
                n.life.streetActive = true;
            }
            distance = Game::Distance(n.p, goal);
            if (distance < .001)
            {
                return false;
            }
        }
        else
        {
            n.life.streetActive = false;
        }
        double x = (goal.x - n.p.x) / distance, y = (goal.y - n.p.y) / distance;
        double step = std::min(distance, dt * (n.life.activity == Raiding ? 67 : 48));
        // Corridors are cleared at generation; local steering handles buildings and travellers.
        for (double angle : {0.0, .65, -.65, 1.2, -1.2, 1.57, -1.57})
        {
            double dx = x * std::cos(angle) - y * std::sin(angle);
            double dy = x * std::sin(angle) + y * std::cos(angle);
            Game::Vec next(n.p.x + dx * step, n.p.y + dy * step);
            if (Game::Walkable(next))
            {
                n.p = next;
                return false;
            }
        }
        return false;
    }

    inline void Attack(Game::World& w, Game::Npc& n, Game::Npc& victim)
    {
        if (n.life.cooldown > 0 || !victim.life.alive)
        {
            return;
        }
        n.life.cooldown = 1.1;
        if (victim.life.target != n.id)
        {
            Record(w, Assault, n.id, victim.id);
        }
        float damage = 6.f + n.power * .13f + (n.ability == Game::Body ? 3 : Secondary(n, Game::Body) ? 1 : 0);
        victim.life.health = std::max(0.f, victim.life.health - damage);
        victim.life.target = n.id;
        if (n.life.activity == Raiding)
        {
            ++w.society.raids;
        }
        if (victim.life.health <= 0)
        {
            Die(w, victim, n.id);
            n.life.target = 0;
        }
    }

    inline void Decide(Game::World& w, Game::Npc& n, bool playerPresent)
    {
        auto& l = n.life;
        if (l.health < l.maxHealth * .35f || (l.goal == Vitality && l.health < l.maxHealth * .9f))
        {
            l.goal = Vitality;
            l.activity = Recovering;
            l.target = 0;
            return;
        }
        l.goal = (l.job == Seeker || (l.insight < 10 && n.echo[3] > n.echo[5])) ? Insight : Wealth;
        bool unsolicited = l.playerProvoked <= 0;
        if (l.target == -1 && (!playerPresent || Game::Distance(n.p, w.player.p) > 180 || l.playerChase >= 6 ||
                               Game::Distance(n.p, n.home) > VillageRadius + 360 ||
                               (unsolicited && (w.society.playerRaidGrace > 0 || Game::VillageAt(w.player.p) >= 0 ||
                                                w.society.gold <= 0))))
        {
            l.target = 0;
            l.playerChase = 0;
            l.playerRetry = 20;
            if (unsolicited)
            {
                w.society.playerRaidGrace = std::max(w.society.playerRaidGrace, 8.0);
            }
        }
        if (auto* target = Find(w, l.target))
        {
            if (!target->life.alive || Game::Distance(n.p, target->p) > 300)
            {
                l.target = 0;
            }
        }
        if (!l.target && (l.raidWeight > .6f || l.job == Guardian))
        {
            double best = -1;
            for (const auto& target : w.npcs)
            {
                double d = Game::Distance(n.p, target.p);
                bool threat = target.life.activity == Raiding && target.life.target != 0;
                bool victim = target.life.gold > 5 && target.life.job != Raider && Game::VillageAt(target.p) < 0;
                if (target.id == n.id || !target.life.alive || target.id == w.story.childId || d > 230 ||
                    !(l.job == Guardian ? threat : victim))
                {
                    continue;
                }
                double score = l.job == Guardian ? 230 - d : target.life.gold * l.raidWeight - d * .08;
                if (score > best)
                {
                    best = score;
                    l.target = target.id;
                }
            }
            if (!l.target && playerPresent && l.job != Guardian && l.raidWeight > .6f && l.playerRetry <= 0 &&
                w.society.playerRaidGrace <= 0 && Game::VillageAt(w.player.p) < 0 && Game::VillageAt(n.p) < 0 &&
                w.society.gold >= 15 && Game::Distance(n.p, w.player.p) < 85 &&
                Game::Distance(n.p, n.home) < VillageRadius + 280 && ClearSegment(n.p, w.player.p) &&
                std::none_of(
                    w.npcs.begin(),
                    w.npcs.end(),
                    [](const Game::Npc& other)
                    {
                        return other.life.alive && other.life.target == -1;
                    }
                ))
            {
                l.target = -1;
                l.playerChase = 0;
                l.cooldown = std::max(l.cooldown, 1.5);
            }
        }
        if (l.target)
        {
            l.activity = l.job == Guardian ? Patrolling : l.raidWeight > .6f ? Raiding : Defending;
            return;
        }
        if (l.goods >= 6 && l.job != Merchant)
        {
            l.activity = Trading;
        }
        else if (l.job == Merchant)
        {
            l.activity = Travelling;
        }
        else if (l.job == Guardian || l.job == Raider)
        {
            l.activity = Patrolling;
        }
        else if (l.job == Healer)
        {
            l.activity = Healing;
        }
        else
        {
            l.activity = l.goal == Insight ? Studying : Hunting;
        }
    }

    inline void TickNpc(Game::World& w, Game::Npc& n, double dt, bool playerPresent)
    {
        InitializeNpc(n);
        auto& l = n.life;
        if (!l.alive)
        {
            return;
        }
        l.cooldown = std::max(0.0, l.cooldown - dt);
        l.playerRetry = std::max(0.0, l.playerRetry - dt);
        l.playerProvoked = std::max(0.0, l.playerProvoked - dt);
        l.playerChase = l.target == -1 ? l.playerChase + dt : 0;
        if (l.movingTo >= 0 && l.health >= l.maxHealth * .35f && !l.target)
        {
            l.activity = Travelling;
            Game::Vec goal = Game::VillageCenter(l.movingTo), previous = n.p;
            if (Move(n, goal, dt))
            {
                std::array<bool, HousingCapacity> occupied = {};
                for (const auto& other : w.npcs)
                {
                    if (other.life.alive && other.life.village == l.movingTo && other.life.houseLot >= 0)
                    {
                        occupied[other.life.houseLot] = true;
                    }
                }
                for (int lot = 0; lot < HousingCapacity; ++lot)
                {
                    if (occupied[lot])
                    {
                        continue;
                    }
                    l.village = l.destination = l.movingTo;
                    l.houseLot = lot;
                    n.home = Game::HouseEntrance(l.village, lot);
                    Record(w, MigrationCompleted, n.id, 0, l.village);
                    std::cout << "[Migration] npc=" << n.id << " arrivedVillage=" << l.village << " house=" << lot
                              << '\n';
                    break;
                }
                l.movingTo = -1;
                l.migrationCooldown = w.society.elapsed + 180;
            }
            n.motion.Update(goal.x - n.p.x, goal.y - n.p.y, n.p.x - previous.x, n.p.y - previous.y);
            return;
        }
        Decide(w, n, playerPresent);
        Game::Vec previous = n.p;
        Game::Vec goal = n.home;
        if (l.target)
        {
            auto* victim = Find(w, l.target);
            goal = victim ? victim->p : w.player.p;
            if (Game::Distance(n.p, goal) > 25 || !ClearSegment(n.p, goal))
            {
                Move(n, goal, dt);
            }
            else if (victim)
            {
                Attack(w, n, *victim);
            }
            else if (playerPresent && l.cooldown <= 0 && w.society.playerHitRecovery <= 0)
            {
                l.cooldown = 2.5;
                w.society.playerHitRecovery = 1.5;
                float damage = l.playerProvoked > 0 ? 6 + n.power * .1f : 3 + n.power * .03f;
                w.player.health = std::max(0.f, w.player.health - damage);
                if (l.activity == Raiding)
                {
                    ++w.society.raids;
                }
                if (w.player.health <= 0)
                {
                    int stolen = std::min(w.society.gold, 1000000 - l.gold);
                    w.society.gold -= stolen;
                    l.gold += stolen;
                    Record(w, Killed, n.id, 0, stolen);
                    Game::Die(w, Story::VillageRaid);
                    // Game::Die may grow npcs: the caller must not keep this reference.
                    return;
                }
            }
        }
        else
        {
            if (l.activity == Hunting)
            {
                goal = Game::HuntingGround(l.village);
            }
            else if (l.activity == Studying)
            {
                goal = Game::MeditationGround(l.village);
            }
            else if (l.activity == Travelling)
            {
                goal = Game::VillageCenter(l.destination);
            }
            else if (l.activity == Patrolling)
            {
                double side = std::sin(w.society.elapsed * .025 + n.id) > 0 ? 1 : -1;
                goal = Game::Vec(n.home.x + side * 175, n.home.y + side * 175);
            }
            bool arrived = Move(n, goal, dt);
            if (arrived)
            {
                l.work = l.activity == Recovering ? 0 : l.work + dt;
                if (l.activity == Recovering)
                {
                    l.health = std::min(l.maxHealth, l.health + (float)dt * 5);
                    if (l.health > l.maxHealth * .9f)
                    {
                        l.goal = Wealth;
                    }
                }
                else if (l.work >= WorkInterval)
                {
                    l.work = 0;
                    if (l.activity == Hunting)
                    {
                        l.goods = std::min(1000, l.goods + (n.ability == Game::Sense ? 3 : 2));
                        l.health = std::max(1.f, l.health - 7);
                        l.maxHealth = std::min(200.f, l.maxHealth + .5f);
                        n.power = std::min(100, n.power + 1);
                        n.echo[0] = std::min(1000000, n.echo[0] + 1);
                        Record(w, HuntCompleted, n.id, 0, 2);
                    }
                    else if (l.activity == Trading)
                    {
                        Sell(w, n, l.village);
                    }
                    else if (l.activity == Travelling)
                    {
                        auto& market = w.society.villages[l.destination];
                        Sell(w, n, l.destination);
                        int quantity =
                            std::min({6, market.stock, l.gold / 2, 1000 - l.goods, (1000000 - market.treasury) / 2});
                        l.gold -= quantity * 2;
                        l.goods += quantity;
                        market.stock -= quantity;
                        market.treasury += quantity * 2;
                        if (quantity)
                        {
                            Record(w, GoodsBought, n.id, 0, quantity);
                        }
                        l.destination = (l.destination + 1) % VillageCount;
                    }
                    else if (l.activity == Studying)
                    {
                        l.insight = std::min(10000, l.insight + 1);
                        n.echo[3] = std::min(1000000, n.echo[3] + 1);
                        if (l.insight % 5 == 0)
                        {
                            n.power = std::min(100, n.power + 3);
                            Record(w, Enlightened, n.id, 0, l.insight);
                        }
                    }
                    else if (l.activity == Healing)
                    {
                        for (auto& patient : w.npcs)
                        {
                            if (patient.life.alive && patient.life.health < patient.life.maxHealth &&
                                Game::Distance(n.p, patient.p) < 100)
                            {
                                float healing = n.ability == Game::Life ? 24.f : Secondary(n, Game::Life) ? 18.f : 16.f;
                                patient.life.health = std::min(patient.life.maxHealth, patient.life.health + healing);
                                n.echo[1] = std::min(1000000, n.echo[1] + 1);
                                l.insight = std::min(10000, l.insight + 1);
                                Record(w, AidGiven, n.id, patient.id, 16);
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                l.work = 0;
            }
        }
        n.motion.Update(goal.x - n.p.x, goal.y - n.p.y, n.p.x - previous.x, n.p.y - previous.y);
    }

    inline void ExchangeNews(Game::World& w)
    {
        auto& s = w.society;
        for (auto it = s.letters.begin(); it != s.letters.end();)
        {
            if (it->arrival > s.elapsed)
            {
                ++it;
                continue;
            }
            auto& destination = s.villages[it->to].news[it->origin];
            if (it->news.time > destination.time)
            {
                destination = it->news;
            }
            for (auto& rumor : s.rumors)
            {
                if (rumor.id == it->rumorId)
                {
                    rumor.reachMask |= 1 << it->to;
                }
            }
            Record(w, LetterDelivered, 0, 0, it->to);
            std::cout << "[Mail] from=" << it->from << " to=" << it->to << " origin=" << it->origin
                      << " rumor=" << it->rumorId << '\n';
            it = s.letters.erase(it);
        }
        if (s.elapsed >= s.nextNews)
        {
            s.nextNews = s.elapsed + 30;
            for (int v = 0; v < VillageCount; ++v)
            {
                News local;
                local.time = s.elapsed;
                local.stock = s.villages[v].stock;
                int raiders = 0;
                for (const auto& npc : w.npcs)
                {
                    if (npc.life.alive && npc.life.village == v)
                    {
                        ++local.residents;
                        raiders += npc.life.raidWeight > .6f;
                    }
                }
                local.danger = local.residents ? (float)raiders / local.residents : 0;
                s.villages[v].news[v] = local;
                for (int to = 0; to < VillageCount; ++to)
                {
                    double distance = Game::Distance(Game::VillageCenter(v), Game::VillageCenter(to));
                    if (to == v || distance > VillageSpacing * 1.01)
                    {
                        continue;
                    }
                    for (int origin = 0; origin < VillageCount && s.letters.size() < 1024; ++origin)
                    {
                        const auto& news = s.villages[v].news[origin];
                        if (news.time < 0 || news.time <= s.villages[to].news[origin].time)
                        {
                            continue;
                        }
                        Letter letter;
                        letter.from = v;
                        letter.to = to;
                        letter.origin = origin;
                        letter.arrival = s.elapsed + distance / 48;
                        letter.news = news;
                        s.letters.push_back(letter);
                    }
                }
            }
        }
        for (auto& npc : w.npcs)
        {
            auto& life = npc.life;
            if (!life.alive)
            {
                continue;
            }
            life.known[life.village] = s.villages[life.village].news[life.village];
            for (const auto& visitor : w.npcs)
            {
                if (!visitor.life.alive || Game::Distance(npc.p, visitor.p) > 65)
                {
                    continue;
                }
                for (int origin = 0; origin < VillageCount; ++origin)
                {
                    if (visitor.life.known[origin].time > life.known[origin].time)
                    {
                        life.known[origin] = visitor.life.known[origin];
                    }
                }
            }
            int here = Game::VillageAt(npc.p);
            if (here >= 0 && Game::Distance(npc.p, Game::VillageCenter(here)) < 140)
            {
                for (int origin = 0; origin < VillageCount; ++origin)
                {
                    auto& bulletin = s.villages[here].news[origin];
                    if (life.known[origin].time > bulletin.time)
                    {
                        bulletin = life.known[origin];
                    }
                    else
                    {
                        life.known[origin] = bulletin;
                    }
                }
                for (auto& rumor : s.rumors)
                {
                    if (rumor.id == life.carriedRumor)
                    {
                        rumor.reachMask |= 1 << here;
                    }
                    if (rumor.reachMask & (1 << here))
                    {
                        life.carriedRumor = std::max(life.carriedRumor, rumor.id);
                    }
                }
            }
            if (life.movingTo >= 0 || life.target || life.migrationCooldown > s.elapsed || npc.id == w.story.childId)
            {
                continue;
            }
            life.migrationCooldown = s.elapsed + 60 + npc.id % 30;
            auto score = [&](int v)
            {
                const auto& news = life.known[v];
                if (news.time < 0 || s.elapsed - news.time > 180)
                {
                    return -100.0;
                }
                double affinity = life.raidWeight > .6f ? news.danger : -news.danger;
                double opportunity = (double)news.stock / std::max(1, news.residents) / 20;
                if (life.job == Seeker && (v == 3 || v == 6))
                {
                    affinity += .5;
                }
                for (const auto& rumor : s.rumors)
                {
                    if (((here >= 0 && (rumor.reachMask & (1 << here))) || rumor.id == life.carriedRumor) &&
                        rumor.village == v && s.elapsed - rumor.time < 180)
                    {
                        affinity -= life.raidWeight > .6f ? 0 : .3;
                    }
                }
                return affinity + std::min(1.0, opportunity);
            };
            // Only settlements described by delivered news can be considered.
            int best = life.village;
            double threshold = score(best) + .35;
            if (life.known[best].time < 0)
            {
                continue;
            }
            for (int v = 0; v < VillageCount; ++v)
            {
                double value = score(v);
                if (value <= threshold)
                {
                    continue;
                }
                int reserved = 0;
                for (const auto& other : w.npcs)
                {
                    reserved += other.life.alive && (other.life.village == v || other.life.movingTo == v);
                }
                if (reserved >= HousingCapacity)
                {
                    continue;
                }
                best = v;
                threshold = value;
            }
            if (best != life.village)
            {
                life.movingTo = life.destination = best;
                Record(w, MigrationStarted, npc.id, 0, best);
                std::cout << "[Migration] npc=" << npc.id << " from=" << life.village << " to=" << best
                          << " newsTime=" << life.known[best].time << '\n';
            }
        }
    }

    inline void Update(Game::World& w, double dt, bool playerPresent)
    {
        if (!std::isfinite(dt) || dt <= 0)
        {
            return;
        }
        Initialize(w);
        auto& s = w.society;
        s.accumulator += std::min(dt, .25);
        while (s.accumulator + 1e-9 >= .1)
        {
            s.accumulator = std::max(0.0, s.accumulator - .1);
            s.elapsed += .1;
            ++s.ticks;
            if (s.ticks % 10 == 0)
            {
                ExchangeNews(w);
            }
            s.playerCooldown = std::max(0.0, s.playerCooldown - .1);
            if (playerPresent)
            {
                s.playerRaidGrace = std::max(0.0, s.playerRaidGrace - .1);
                s.playerHitRecovery = std::max(0.0, s.playerHitRecovery - .1);
            }
            for (auto& village : s.villages)
            {
                village.production += .1;
                if (village.production >= 20)
                {
                    village.production -= 20;
                    village.stock = std::min(1000, village.stock + 20);
                    int index = (int)(&village - s.villages.data());
                    for (auto& n : w.npcs)
                    {
                        if (n.life.alive && n.life.village == index && n.life.gold >= 4 && village.stock > 0 &&
                            village.treasury <= 999996)
                        {
                            n.life.gold -= 4;
                            village.treasury += 4;
                            --village.stock;
                            n.life.health = std::min(n.life.maxHealth, n.life.health + 5);
                        }
                    }
                }
            }
            // Index iteration is deliberate: a player death can append a successor during this tick.
            const size_t count = w.npcs.size();
            for (size_t i = 0; i < count; ++i)
            {
                TickNpc(w, w.npcs[i], .1, playerPresent);
            }
            Births(w);
            w.dirty = true;
        }
        if (s.elapsed >= s.logAt)
        {
            s.logAt = s.elapsed + 10;
            size_t alive = 0;
            size_t playerPursuers = 0;
            long long gold = s.gold;
            std::array<int, 9> activities = {};
            for (const auto& n : w.npcs)
            {
                gold += n.life.gold;
                if (n.life.alive)
                {
                    ++alive;
                    playerPursuers += n.life.target == -1;
                    ++activities[n.life.activity];
                }
            }
            for (const auto& village : s.villages)
            {
                gold += village.treasury;
            }
            std::cout << "[World] tick=" << s.ticks << " alive=" << alive << " records=" << w.npcs.size()
                      << " hunting=" << activities[Hunting]
                      << " trading=" << activities[Trading] + activities[Travelling]
                      << " fighting=" << activities[Raiding] << " studying=" << activities[Studying]
                      << " trades=" << s.trades << " raidHits=" << s.raids << " deaths=" << s.deaths
                      << " totalGold=" << gold << " playerPursuers=" << playerPursuers
                      << " playerRaidGrace=" << s.playerRaidGrace << " simulationHz=10\n";
        }
    }

    inline bool CanTrade(const Game::World& w, const Game::Npc& n)
    {
        return n.life.alive && n.life.job == Merchant && n.life.target == 0 && n.relationship >= 0 &&
               Game::Distance(n.p, w.player.p) < 70 && Game::VillageAt(n.p) >= 0 &&
               Game::VillageAt(n.p) == Game::VillageAt(w.player.p);
    }

    inline bool PlayerTrade(Game::World& w, Game::Npc& n, bool buy)
    {
        if (!CanTrade(w, n))
        {
            return false;
        }
        if (buy)
        {
            if (n.life.goods <= 0 || w.society.gold < 4 || n.life.gold > 999996 || w.player.bread >= 1000)
            {
                return false;
            }
            --n.life.goods;
            ++w.player.bread;
            w.society.gold -= 4;
            n.life.gold += 4;
        }
        else
        {
            if (w.player.herbs <= 0 || n.life.gold < 3 || n.life.goods >= 1000 || w.society.gold > 999997)
            {
                return false;
            }
            --w.player.herbs;
            ++n.life.goods;
            w.society.gold += 3;
            n.life.gold -= 3;
        }
        ++w.society.trades;
        Record(w, buy ? GoodsBought : GoodsSold, 0, n.id, 1);
        return true;
    }

    inline bool PlayerAttack(Game::World& w, int id)
    {
        auto* n = Find(w, id);
        if (!n || !n->life.alive || w.society.playerCooldown > 0 || Game::Distance(n->p, w.player.p) > 65 ||
            !ClearSegment(w.player.p, n->p) || (id == w.story.childId && !w.story.childComplete))
        {
            return false;
        }
        w.society.playerCooldown = .65;
        n->relationship = std::max(-100, n->relationship - 15);
        n->life.target = -1;
        n->life.playerProvoked = 12;
        n->life.playerChase = 0;
        n->life.cooldown = std::max(n->life.cooldown, .8);
        n->life.health = std::max(0.f, n->life.health - 22);
        w.player.echo[0] += 2;
        ++w.player.echo[5];
        Record(w, Assault, 0, id, 22);
        for (auto& guard : w.npcs)
        {
            if (guard.life.alive && guard.life.job == Guardian && Game::Distance(guard.p, w.player.p) < 200)
            {
                guard.life.target = -1;
                guard.life.playerProvoked = 12;
                guard.life.playerChase = 0;
                guard.life.cooldown = std::max(guard.life.cooldown, .8);
            }
        }
        if (n->life.health <= 0)
        {
            Die(w, *n, 0);
        }
        return true;
    }

    inline std::wstring Status(const Game::Npc& n)
    {
        const auto& l = n.life;
        return std::wstring(JobName(l.job)) + L" / " + ActivityName(l.activity) + L" / " + GoalName(l.goal) +
               L"\n\uccb4\ub825 " + std::to_wstring((int)l.health) + L"/" + std::to_wstring((int)l.maxHealth) +
               L"  \uc740\ud654 " + std::to_wstring(l.gold) + L"  \ubb3c\ud488 " + std::to_wstring(l.goods) +
               L"  \uae68\ub2ec\uc74c " + std::to_wstring(l.insight);
    }

    inline std::wstring EventText(const Event& e)
    {
        static const wchar_t* names[] = {
            L"\uc0ac\ub0e5 \uc218\ud655",
            L"\ubb3c\ud488 \ud310\ub9e4",
            L"\ubb3c\ud488 \uad6c\ub9e4",
            L"\uacf5\uaca9",
            L"\uc0ac\ub9dd",
            L"\uc0c8\ub85c\uc6b4 \uacc4\uc2b9\uc790",
            L"\uae68\ub2ec\uc74c",
            L"\uce58\ub8cc",
            L"\ud3b8\uc9c0 \ub3c4\ucc29",
            L"\uc774\uc8fc \ucd9c\ubc1c",
            L"\uc774\uc8fc \uc644\ub8cc"
        };
        return (e.actor ? L"#" + std::to_wstring(e.actor) : L"\ub2f9\uc2e0") + std::wstring(L"  ") + names[e.kind] +
               (e.other ? L"  #" + std::to_wstring(e.other) : L"");
    }
}
