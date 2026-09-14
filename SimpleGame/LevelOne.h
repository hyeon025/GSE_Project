#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <istream>
#include <ostream>
#include <queue>
#include <random>
#include <string>
#include <vector>

namespace LevelOne
{
    constexpr int MapSize = 31;
    constexpr int CellCount = MapSize * MapSize;
    constexpr float CellSize = 48.0f;
    constexpr int Center = MapSize / 2;
    constexpr int KillGoal = 24;
    constexpr int SoulGoal = 12;

    struct Point
    {
        double x = 0;
        double y = 0;

        Point(double px = 0, double py = 0)
            : x(px),
              y(py)
        {
        }
    };

    inline double Distance(Point a, Point b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    inline Point Direction(Point from, Point to)
    {
        double length = Distance(from, to);
        return length > 0.0001 ? Point((to.x - from.x) / length, (to.y - from.y) / length) : Point();
    }

    inline Point CenterOf(int x, int y)
    {
        return Point((x - Center) * CellSize, (y - Center) * CellSize);
    }

    inline int CellOf(Point p)
    {
        int x = (int)std::floor(p.x / CellSize + Center + 0.5);
        int y = (int)std::floor(p.y / CellSize + Center + 0.5);
        return x >= 0 && x < MapSize && y >= 0 && y < MapSize ? y * MapSize + x : -1;
    }

    enum Phase
    {
        Fighting,
        Defeated,
        Cleared
    };

    enum DropKind
    {
        Soul,
        Weapon,
        Healing,
        Magnet
    };

    enum EnemyKind
    {
        Raider,
        Runner,
        Archer,
        Boss
    };

    struct Enemy
    {
        int id = 0;
        int kind = Raider;
        Point p;
        float health = 26;
        float maxHealth = 26;
        float attackTimer = 1;
        float windup = 0;
        float spawnTime = 0.8f;
        float flash = 0;
        int attack = 0;
        Point aim;
    };

    struct Projectile
    {
        Point p;
        Point direction;
        float remaining = 0;
        float damage = 0;
        float speed = 0;
        bool hostile = false;
    };

    struct Drop
    {
        Point p;
        int kind = Soul;
        int amount = 1;
        bool attracted = false;
    };

    struct HitEffect
    {
        Point p;
        float time = 0.25f;
        bool healing = false;
    };

    struct Session
    {
        unsigned seed = 0;
        std::mt19937 random;
        bool active = true;
        bool completedOnce = false;
        int phase = Fighting;
        int level = 1;
        int xp = 0;
        int weapon = 0;
        int kills = 0;
        int souls = 0;
        int upgrades = 0;
        int nextEnemyId = 1;
        int bossState = 0;
        float bossTimer = 0;
        float elapsed = 0;
        float health = 100;
        float stamina = 100;
        float shotTimer = 0;
        float invulnerable = 0;
        float spawnTimer = 2.5f;
        float magnetTimer = 0;
        Point p;
        std::vector<Enemy> enemies;
        std::vector<Projectile> projectiles;
        std::vector<Drop> drops;

        // Derived navigation and short visual feedback do not belong in the save file.
        std::array<unsigned char, CellCount> blocked = {};
        std::array<int, CellCount> flow = {};
        float flowTimer = 0;
        int targetId = -1;
        float levelFlash = 0;
        std::vector<HitEffect> effects;
        std::wstring notice;
        float noticeTimer = 0;
        int encounter = 0;
        int dominantTrait = 6, allies = 0, opponents = 0;
        bool judgementPending = false, judgementSeen = false, revivalUsed = false;
        bool phaseTwo = false;
        float supportTimer = 0;
        std::vector<Point> memories;
    };

    inline float MaxHealth(const Session& s)
    {
        return 100.0f + (s.level - 1) * 12;
    }

    inline float Damage(const Session& s)
    {
        float weapon = (float)s.weapon;
        if (s.encounter == 3 && s.dominantTrait == 5)
        {
            for (const auto& enemy : s.enemies)
            {
                if (enemy.kind == Boss && enemy.health <= enemy.maxHealth * .5f)
                {
                    weapon *= .25f;
                }
            }
        }
        float insight = s.encounter == 3 && s.phaseTwo && s.dominantTrait == 3 ? 1.25f : 1.f;
        return (12.0f + (s.level - 1) * 3 + weapon * 4) * (1 + s.allies * .05f) * insight;
    }

    inline const wchar_t* AreaName(const Session& s)
    {
        return s.encounter == 1   ? L"\ubcc0\ubc29 \ub9c8\uc744 \u00b7 \uc2b5\uaca9"
               : s.encounter == 2 ? L"\uc131\uacc4\uad50\ub2e8 \u00b7 \ubd89\uc740 \uc608\ubc30\ub2f9"
               : s.encounter == 3 ? L"\ud0dc\ucd08\uc758 \ubb18\uc9c0"
                                  : L"\ub808\ubca8 1 \u00b7 \uc7bf\ube5b \uacbd\uc791\uc9c0";
    }

    inline const wchar_t* BossName(const Session& s)
    {
        return s.encounter == 2   ? L"\ubd89\uc740 \uc131\uc790 \uc544\ubca8\ub860"
               : s.encounter == 3 ? L"\ucd5c\ucd08\uc758 \uacc4\uc2b9\uc790 \uc5d0\ub179"
                                  : L"\uace1\ucc3d\uc758 \ud30c\uc218\uafbc";
    }

    inline float Cooldown(const Session& s)
    {
        return std::max(0.18f, 0.78f * std::pow(0.94f, (float)(s.level - 1)));
    }

    inline float Range(const Session& s)
    {
        return 260.0f + std::min(80, (s.level - 1) * 5);
    }

    inline float PickupRadius(const Session& s)
    {
        return s.magnetTimer > 0 ? 620.0f : 76.0f + (s.level - 1) * 3;
    }

    inline float MoveSpeed(const Session& s)
    {
        return 100.0f + std::min(20, s.level - 1);
    }

    inline int NextXp(const Session& s)
    {
        return 18 + (s.level - 1) * 9;
    }

    inline void Notice(Session& s, const std::wstring& text)
    {
        s.notice = text;
        s.noticeTimer = 4;
    }

    inline std::array<int, CellCount> Flood(const Session& s, int start)
    {
        std::array<int, CellCount> distance;
        distance.fill(-1);
        if (start < 0 || start >= CellCount || s.blocked[start])
        {
            return distance;
        }
        std::queue<int> queue;
        queue.push(start);
        distance[start] = 0;
        while (!queue.empty())
        {
            int at = queue.front();
            queue.pop();
            int x = at % MapSize;
            int y = at / MapSize;
            const int dx[] = {1, -1, 0, 0};
            const int dy[] = {0, 0, 1, -1};
            for (int i = 0; i < 4; ++i)
            {
                int nx = x + dx[i];
                int ny = y + dy[i];
                if (nx < 0 || ny < 0 || nx >= MapSize || ny >= MapSize)
                {
                    continue;
                }
                int next = ny * MapSize + nx;
                if (!s.blocked[next] && distance[next] < 0)
                {
                    distance[next] = distance[at] + 1;
                    queue.push(next);
                }
            }
        }
        return distance;
    }

    inline void GenerateMap(Session& s)
    {
        std::mt19937 generator(s.seed);
        for (int y = 0; y < MapSize; ++y)
        {
            for (int x = 0; x < MapSize; ++x)
            {
                int dx = std::abs(x - Center);
                int dy = std::abs(y - Center);
                bool boundary = x == 0 || y == 0 || x == MapSize - 1 || y == MapSize - 1;
                bool road = dx <= 1 || dy <= 1 || std::abs(std::max(dx, dy) - 9) <= 1;
                bool clearing = (dx <= 3 && dy <= 3) || (dx <= 4 && std::abs(y - (Center + 9)) <= 4);
                s.blocked[y * MapSize + x] = boundary || (!road && !clearing && generator() % 100 < 24);
            }
        }
        // Connect every floor island to the spawn. A 48-unit cell clears the largest actor diameter (40).
        auto reach = Flood(s, Center * MapSize + Center);
        for (int y = 1; y < MapSize - 1; ++y)
        {
            for (int x = 1; x < MapSize - 1; ++x)
            {
                int at = y * MapSize + x;
                if (!s.blocked[at] && reach[at] < 0)
                {
                    int cx = x;
                    int cy = y;
                    while (cx != Center)
                    {
                        s.blocked[cy * MapSize + cx] = 0;
                        cx += cx < Center ? 1 : -1;
                    }
                    while (cy != Center)
                    {
                        s.blocked[cy * MapSize + cx] = 0;
                        cy += cy < Center ? 1 : -1;
                    }
                    reach = Flood(s, Center * MapSize + Center);
                }
            }
        }
        s.flow = Flood(s, CellOf(s.p));
        s.flowTimer = 0;
    }

    inline bool Walkable(const Session& s, Point p, float radius)
    {
        int cell = CellOf(p);
        if (cell < 0)
        {
            return false;
        }
        int cx = cell % MapSize;
        int cy = cell / MapSize;
        for (int y = cy - 1; y <= cy + 1; ++y)
        {
            for (int x = cx - 1; x <= cx + 1; ++x)
            {
                if (x < 0 || y < 0 || x >= MapSize || y >= MapSize)
                {
                    return false;
                }
                if (!s.blocked[y * MapSize + x])
                {
                    continue;
                }
                Point center = CenterOf(x, y);
                double dx = std::max(std::abs(p.x - center.x) - CellSize * 0.5, 0.0);
                double dy = std::max(std::abs(p.y - center.y) - CellSize * 0.5, 0.0);
                if (dx * dx + dy * dy < radius * radius)
                {
                    return false;
                }
            }
        }
        return true;
    }

    inline bool ClearPath(const Session& s, Point a, Point b, float radius = 3)
    {
        int steps = std::max(1, (int)std::ceil(Distance(a, b) / 6));
        for (int i = 0; i <= steps; ++i)
        {
            double t = (double)i / steps;
            if (!Walkable(s, Point(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t), radius))
            {
                return false;
            }
        }
        return true;
    }

    inline void Move(const Session& s, Point& p, Point delta, float radius)
    {
        int steps = std::max(1, (int)std::ceil(std::hypot(delta.x, delta.y) / 6));
        for (int i = 0; i < steps; ++i)
        {
            Point next(p.x + delta.x / steps, p.y);
            if (Walkable(s, next, radius))
            {
                p = next;
            }
            next = Point(p.x, p.y + delta.y / steps);
            if (Walkable(s, next, radius))
            {
                p = next;
            }
        }
    }

    inline Point ChasePoint(const Session& s, Point p, float radius)
    {
        if (ClearPath(s, p, s.p, radius))
        {
            return s.p;
        }
        int at = CellOf(p);
        if (at < 0)
        {
            return p;
        }
        int best = at;
        for (int next : {at - 1, at + 1, at - MapSize, at + MapSize})
        {
            if (next >= 0 && next < CellCount && s.flow[next] >= 0 && (s.flow[best] < 0 || s.flow[next] < s.flow[best]))
            {
                best = next;
            }
        }
        Point next = CenterOf(best % MapSize, best / MapSize);
        if (!ClearPath(s, p, next, radius))
        {
            return CenterOf(at % MapSize, at / MapSize);
        }
        return next;
    }

    inline void AddXp(Session& s, int amount)
    {
        if (s.level >= 30)
        {
            return;
        }
        s.xp += amount;
        while (s.level < 30 && s.xp >= NextXp(s))
        {
            s.xp -= NextXp(s);
            ++s.level;
            s.health = std::min(MaxHealth(s), s.health + 28);
            s.levelFlash = 1.2f;
            Notice(
                s,
                L"\ub808\ubca8 \uc0c1\uc2b9 \u00b7 \uacf5\uaca9\ub825, \ubc1c\uc0ac \uc18d\ub3c4, \ucd5c\ub300 \uccb4\ub825\uc774 \uc99d\uac00\ud588\uc2b5\ub2c8\ub2e4."
            );
        }
        if (s.level == 30)
        {
            s.xp = 0;
        }
    }

    inline void AddDrop(Session& s, Point p, int kind, int amount = 1)
    {
        for (auto& drop : s.drops)
        {
            if (drop.kind == kind && Distance(drop.p, p) < 20)
            {
                drop.amount += amount;
                return;
            }
        }
        s.drops.push_back({p, kind, amount, false});
    }

    inline void SpawnEnemy(Session& s, int kind, Point p)
    {
        Enemy enemy;
        enemy.id = s.nextEnemyId++;
        enemy.kind = kind;
        enemy.p = p;
        enemy.maxHealth = kind == Boss ? 680 : kind == Runner ? 22 : kind == Archer ? 34 : 28;
        if (kind != Boss)
        {
            enemy.maxHealth += std::min(20.0f, s.elapsed * 0.08f);
        }
        enemy.health = enemy.maxHealth;
        enemy.attackTimer = kind == Boss ? 2.5f : 1.5f;
        s.enemies.push_back(enemy);
    }

    inline void Start(Session& s, unsigned seed, bool keepGrowth = true)
    {
        int level = keepGrowth ? s.level : 1;
        int xp = keepGrowth ? s.xp : 0;
        int weapon = keepGrowth ? s.weapon : 0;
        bool completed = s.completedOnce;
        s = Session();
        s.seed = seed ? seed : 1;
        s.random.seed(s.seed ^ 0xa17e391u);
        s.level = level;
        s.xp = xp;
        s.weapon = weapon;
        s.completedOnce = completed;
        s.health = MaxHealth(s);
        GenerateMap(s);
        // The opening route guarantees a first pickup without depending on a random drop.
        AddDrop(s, Point(50, 50), Soul);
        AddDrop(s, Point(90, 90), Soul);
        SpawnEnemy(s, Raider, CenterOf(Center - 3, Center - 3));
        SpawnEnemy(s, Raider, CenterOf(Center + 3, Center + 3));
        Notice(s, L"\ub808\ubca8 1 \u00b7 \uc7bf\ube5b \uacbd\uc791\uc9c0");
    }

    inline void Hurt(Session& s, float damage)
    {
        if (s.invulnerable > 0 || s.phase != Fighting)
        {
            return;
        }
        s.health = std::max(0.0f, s.health - damage);
        s.invulnerable = 0.85f;
        s.effects.push_back({s.p, 0.3f, false});
        if (s.health <= 0)
        {
            if (s.encounter == 3 && !s.revivalUsed && s.allies > 0 && (s.dominantTrait == 6 || s.dominantTrait == 4))
            {
                s.revivalUsed = true;
                s.health = MaxHealth(s) * .4f;
                Notice(
                    s,
                    s.dominantTrait == 6
                        ? L"\uacc4\uc2b9\uc790\ub4e4\uc774 \ub2f9\uc2e0\uc744 \ub2e4\uc2dc \uc77c\uc73c\ud0a8\ub2e4."
                        : L"\uacc4\uc2b9\uc790\uac00 \ubab8\uc744 \ub358\uc838 \uce58\uba85\uc0c1\uc744 \ub9c9\uc558\ub2e4."
                );
                return;
            }
            s.phase = Defeated;
        }
    }

    inline void Shoot(Session& s, Point from, Point toward, bool hostile, float damage, float range, float speed)
    {
        Point direction = Direction(from, toward);
        if (std::hypot(direction.x, direction.y) < 0.5)
        {
            direction = Point(1, 0);
        }
        s.projectiles.push_back({from, direction, range, damage, speed, hostile});
    }

    inline void Kill(Session& s, Enemy& enemy)
    {
        ++s.kills;
        AddDrop(s, enemy.p, Soul, enemy.kind == Boss ? 10 : 1);
        if (enemy.kind == Boss)
        {
            s.bossState = 3;
            s.phase = Cleared;
            s.completedOnce = true;
            s.magnetTimer = 30;
            AddDrop(s, enemy.p, Weapon, 2);
            AddDrop(s, enemy.p, Healing, 2);
            Notice(s, std::wstring(BossName(s)) + L"\uc744 \uc4f0\ub7ec\ub728\ub838\ub2e4.");
        }
        else
        {
            if (s.kills % 5 == 0)
            {
                AddDrop(s, Point(enemy.p.x + 8, enemy.p.y), Weapon);
            }
            if (s.kills % 6 == 0 || s.random() % 100 < 12)
            {
                AddDrop(s, enemy.p, Healing);
            }
            if (s.kills % 9 == 0)
            {
                AddDrop(s, enemy.p, Magnet);
            }
        }
        s.effects.push_back({enemy.p, 0.3f, false});
    }

    inline void CollectDrops(Session& s, float dt, bool collectAll = false)
    {
        for (auto& drop : s.drops)
        {
            if (drop.amount <= 0)
            {
                continue;
            }
            if (drop.kind == Healing && s.health >= MaxHealth(s) && !collectAll)
            {
                continue;
            }
            double d = Distance(s.p, drop.p);
            if (d <= PickupRadius(s) || collectAll)
            {
                drop.attracted = true;
            }
            if (!drop.attracted)
            {
                continue;
            }
            if (d <= 14 || collectAll)
            {
                switch (drop.kind)
                {
                    case Soul:
                        s.souls += drop.amount;
                        AddXp(s, drop.amount * 6);
                        break;
                    case Weapon:
                        s.weapon = std::min(20, s.weapon + drop.amount);
                        s.upgrades += drop.amount;
                        Notice(
                            s,
                            L"\ubb34\uae30 \uac15\ud654 \u00b7 \ubc1c\uc0ac\uccb4 \ud53c\ud574\ub7c9\uc774 \uc99d\uac00\ud588\uc2b5\ub2c8\ub2e4."
                        );
                        break;
                    case Healing:
                        s.health = std::min(MaxHealth(s), s.health + drop.amount * 28);
                        s.effects.push_back({s.p, 0.35f, true});
                        Notice(s, L"\ud68c\ubcf5\uc57d\uc744 \ud761\uc218\ud588\uc2b5\ub2c8\ub2e4.");
                        break;
                    case Magnet:
                        s.magnetTimer = std::max(s.magnetTimer, 10.0f);
                        Notice(
                            s,
                            L"\ud761\uc778\uc11d \u00b7 \uba40\ub9ac \uc788\ub294 \uc544\uc774\ud15c\uc774 \ub04c\ub824\uc635\ub2c8\ub2e4."
                        );
                        break;
                }
                drop.amount = 0;
            }
            else
            {
                Point direction = Direction(drop.p, s.p);
                double step = std::min(d, (190 + d * 1.8) * dt);
                drop.p.x += direction.x * step;
                drop.p.y += direction.y * step;
            }
        }
        s.drops.erase(
            std::remove_if(
                s.drops.begin(),
                s.drops.end(),
                [](const Drop& d)
                {
                    return d.amount <= 0;
                }
            ),
            s.drops.end()
        );
    }

    inline void Update(Session& s, float dt, Point input, bool sprint)
    {
        s.noticeTimer = std::max(0.0f, s.noticeTimer - dt);
        s.levelFlash = std::max(0.0f, s.levelFlash - dt);
        for (auto& effect : s.effects)
        {
            effect.time -= dt;
        }
        s.effects.erase(
            std::remove_if(
                s.effects.begin(),
                s.effects.end(),
                [](const HitEffect& e)
                {
                    return e.time <= 0;
                }
            ),
            s.effects.end()
        );
        if (s.phase != Fighting || s.judgementPending)
        {
            return;
        }
        s.elapsed += dt;
        if (s.encounter == 3 && !s.phaseTwo)
        {
            for (const auto& enemy : s.enemies)
            {
                if (enemy.kind == Boss && enemy.health <= enemy.maxHealth * .5f)
                {
                    s.phaseTwo = true;
                    break;
                }
            }
            if (s.phaseTwo)
            {
                Notice(
                    s,
                    L"\uc5d0\ub179\uc774 \ub2f9\uc2e0\uc774 \ub0a8\uae34 \uc0b6\uc758 \ubc29\ud5a5\uc744 \uc77d\ub294\ub2e4."
                );
                if (s.dominantTrait == 0)
                {
                    SpawnEnemy(s, Raider, Point(144, 0));
                    SpawnEnemy(s, Raider, Point(-144, 0));
                    SpawnEnemy(s, Raider, Point(0, -144));
                }
                else if (s.dominantTrait == 1)
                {
                    AddDrop(s, s.p, Healing, 3);
                }
            }
        }
        if (s.encounter == 1 && s.elapsed >= 45)
        {
            s.invulnerable = 0;
            Hurt(s, MaxHealth(s));
            return;
        }
        if (s.encounter == 1 && s.elapsed >= 30 && s.elapsed - dt < 30)
        {
            Notice(
                s,
                L"\uc131\ubb38\uc774 \ubb34\ub108\uc84c\ub2e4. \ub4a4\ud3b8\uc5d0\uc11c \ub610 \ub2e4\ub978 \uc2b5\uaca9\ub300\uac00 \ubc00\ub824\uc628\ub2e4."
            );
        }
        s.supportTimer = std::max(0.f, s.supportTimer - dt);
        if (s.encounter == 3 && s.allies > 0 && s.supportTimer <= 0)
        {
            s.health = std::min(MaxHealth(s), s.health + s.allies * 2);
            s.supportTimer = 8;
            s.effects.push_back({s.p, .35f, true});
        }
        s.invulnerable = std::max(0.0f, s.invulnerable - dt);
        s.magnetTimer = std::max(0.0f, s.magnetTimer - dt);
        s.shotTimer = std::max(0.0f, s.shotTimer - dt);
        double length = std::hypot(input.x, input.y);
        if (length > 0)
        {
            bool running = sprint && s.stamina > 2;
            float speed = MoveSpeed(s) * (running ? 1.45f : 1.0f);
            Move(s, s.p, Point(input.x / length * speed * dt, input.y / length * speed * dt), 10);
            s.stamina = std::max(0.0f, std::min(100.0f, s.stamina + dt * (running ? -20 : 8)));
        }
        else
        {
            s.stamina = std::min(100.0f, s.stamina + dt * 14);
        }

        s.flowTimer -= dt;
        if (s.flowTimer <= 0)
        {
            s.flow = Flood(s, CellOf(s.p));
            s.flowTimer = 0.2f;
        }

        s.spawnTimer -= dt;
        if (s.spawnTimer <= 0 && s.enemies.size() < 36)
        {
            s.spawnTimer =
                s.bossState == 2 ? std::max(1.f, 4.f - s.opponents * .3f) : std::max(0.75f, 2.0f - s.elapsed * 0.009f);
            for (int tries = 0; tries < 80; ++tries)
            {
                int cell = 1 + s.random() % (CellCount - 2);
                Point p = CenterOf(cell % MapSize, cell / MapSize);
                double d = Distance(p, s.p);
                if (s.blocked[cell] || s.flow[cell] < 0 || d < 250 || d > 520 || !Walkable(s, p, 14))
                {
                    continue;
                }
                bool occupied = false;
                for (const auto& e : s.enemies)
                {
                    if (Distance(e.p, p) < 40)
                    {
                        occupied = true;
                    }
                }
                if (occupied)
                {
                    continue;
                }
                int roll = s.random() % 10;
                int kind = s.elapsed > 25 && roll < 2 ? Archer : roll < 4 ? Runner : Raider;
                SpawnEnemy(s, kind, p);
                break;
            }
        }

        if (s.encounter == 0 && s.bossState == 0 && s.kills >= KillGoal && s.souls >= SoulGoal && s.upgrades > 0 &&
            s.level >= 4)
        {
            s.bossState = 1;
            s.bossTimer = 3.0f;
            Notice(
                s,
                L"\uace1\ucc3d\uc5d0\uc11c \ubb34\uac70\uc6b4 \ubc1c\uc18c\ub9ac\uac00 \ub4e4\ub9bd\ub2c8\ub2e4."
            );
        }
        if (s.bossState == 1)
        {
            s.bossTimer -= dt;
            if (s.bossTimer <= 0)
            {
                s.bossState = 2;
                SpawnEnemy(s, Boss, CenterOf(Center, Center + 9));
            }
        }

        for (auto& enemy : s.enemies)
        {
            enemy.flash = std::max(0.0f, enemy.flash - dt);
            enemy.spawnTime = std::max(0.0f, enemy.spawnTime - dt);
            if (enemy.spawnTime > 0 || enemy.health <= 0)
            {
                continue;
            }
            double d = Distance(enemy.p, s.p);
            enemy.attackTimer -= dt;
            if (enemy.windup > 0)
            {
                enemy.windup -= dt;
                if (enemy.windup <= 0)
                {
                    if (enemy.kind == Boss && enemy.attack % 2 == 0)
                    {
                        if (Distance(s.p, enemy.aim) <= 90)
                        {
                            Hurt(s, 24);
                        }
                        s.effects.push_back({enemy.aim, 0.4f, false});
                    }
                    else if (enemy.kind == Boss)
                    {
                        for (int i = 0; i < 10; ++i)
                        {
                            double a = i * 6.2831853 / 10;
                            Shoot(
                                s,
                                enemy.p,
                                Point(enemy.p.x + std::cos(a), enemy.p.y + std::sin(a)),
                                true,
                                16,
                                620,
                                135
                            );
                        }
                    }
                    else
                    {
                        Shoot(s, enemy.p, enemy.aim, true, 10, 430, 165);
                    }
                    enemy.attackTimer =
                        enemy.kind == Boss ? (enemy.health < enemy.maxHealth * 0.5f ? 1.5f : 2.8f) : 2.7f;
                    ++enemy.attack;
                }
            }
            else
            {
                if ((enemy.kind == Boss && d < 480 || enemy.kind == Archer && d < 320) && enemy.attackTimer <= 0)
                {
                    enemy.aim = s.p;
                    if (enemy.kind == Boss && s.encounter == 3)
                    {
                        enemy.aim.x += input.x * 24;
                        enemy.aim.y += input.y * 24;
                        if (s.phaseTwo && s.dominantTrait == 2 && !s.memories.empty())
                        {
                            enemy.aim = s.memories[(int)(s.elapsed / 4) % s.memories.size()];
                            enemy.attack &= ~1;
                        }
                        if (enemy.health <= enemy.maxHealth * .5f && s.dominantTrait == 0)
                        {
                            enemy.attack |= 1;
                        }
                    }
                    enemy.windup = enemy.kind == Boss ? 1.2f : 0.75f;
                }
                else if (enemy.kind != Archer || d > 190 || !ClearPath(s, enemy.p, s.p))
                {
                    float radius = enemy.kind == Boss ? 20.0f : 12.0f;
                    Point direction = Direction(enemy.p, ChasePoint(s, enemy.p, radius));
                    float speed = enemy.kind == Boss     ? 42.0f
                                  : enemy.kind == Runner ? 66.0f
                                  : enemy.kind == Archer ? 35.0f
                                                         : 43.0f;
                    if (d > radius + 8)
                    {
                        Move(s, enemy.p, Point(direction.x * speed * dt, direction.y * speed * dt), radius);
                    }
                }
            }
            if (Distance(enemy.p, s.p) < (enemy.kind == Boss ? 31 : 23))
            {
                Hurt(s, enemy.kind == Boss ? 18.0f : 9.0f);
            }
        }
        if (s.phase == Defeated)
        {
            return;
        }

        double nearest = Range(s);
        Enemy* target = nullptr;
        s.targetId = -1;
        for (auto& enemy : s.enemies)
        {
            double d = Distance(s.p, enemy.p);
            if (enemy.health > 0 && enemy.spawnTime <= 0 && d <= nearest && ClearPath(s, s.p, enemy.p))
            {
                nearest = d;
                target = &enemy;
            }
        }
        if (target)
        {
            s.targetId = target->id;
            if (s.shotTimer <= 0)
            {
                Shoot(s, s.p, target->p, false, Damage(s), Range(s), 430);
                s.shotTimer = Cooldown(s);
            }
        }

        for (auto& shot : s.projectiles)
        {
            double distance = std::min(shot.remaining, shot.speed * dt);
            int steps = std::max(1, (int)std::ceil(distance / 4));
            for (int i = 0; i < steps && shot.remaining > 0; ++i)
            {
                shot.p.x += shot.direction.x * distance / steps;
                shot.p.y += shot.direction.y * distance / steps;
                shot.remaining -= (float)(distance / steps);
                if (!Walkable(s, shot.p, 2))
                {
                    shot.remaining = 0;
                    break;
                }
                if (shot.hostile)
                {
                    if (Distance(shot.p, s.p) < 13)
                    {
                        Hurt(s, shot.damage);
                        shot.remaining = 0;
                    }
                }
                else
                {
                    for (auto& enemy : s.enemies)
                    {
                        if (enemy.health <= 0 || enemy.spawnTime > 0)
                        {
                            continue;
                        }
                        if (Distance(shot.p, enemy.p) < (enemy.kind == Boss ? 23 : 15))
                        {
                            enemy.health -= shot.damage;
                            enemy.flash = 0.18f;
                            shot.remaining = 0;
                            if (enemy.kind == Boss && s.encounter == 3 && !s.judgementSeen &&
                                enemy.health <= enemy.maxHealth * .1f)
                            {
                                enemy.health = enemy.maxHealth * .1f;
                                s.judgementPending = true;
                                s.invulnerable = .85f;
                            }
                            if (enemy.health <= 0)
                            {
                                Kill(s, enemy);
                            }
                            break;
                        }
                    }
                }
                if (s.phase != Fighting)
                {
                    break;
                }
            }
            if (s.phase != Fighting || s.judgementPending)
            {
                break;
            }
        }
        s.projectiles.erase(
            std::remove_if(
                s.projectiles.begin(),
                s.projectiles.end(),
                [](const Projectile& p)
                {
                    return p.remaining <= 0;
                }
            ),
            s.projectiles.end()
        );
        s.enemies.erase(
            std::remove_if(
                s.enemies.begin(),
                s.enemies.end(),
                [](const Enemy& e)
                {
                    return e.health <= 0;
                }
            ),
            s.enemies.end()
        );
        if (s.phase == Defeated)
        {
            return;
        }
        CollectDrops(s, dt, s.phase == Cleared);
        if (s.phase == Cleared)
        {
            s.enemies.clear();
            s.projectiles.clear();
            s.health = MaxHealth(s);
        }
    }

    inline void Write(std::ostream& out, const Session& s, bool extended = false)
    {
        out << s.seed << ' ' << s.active << ' ' << s.completedOnce << ' ' << s.phase << ' ' << s.level << ' ' << s.xp
            << ' ' << s.weapon << ' ' << s.kills << ' ' << s.souls << ' ' << s.upgrades << ' ' << s.nextEnemyId << ' '
            << s.bossState << ' ' << s.bossTimer << ' ' << s.elapsed << ' ' << s.health << ' ' << s.stamina << ' '
            << s.shotTimer << ' ' << s.invulnerable << ' ' << s.spawnTimer << ' ' << s.magnetTimer << ' ' << s.p.x
            << ' ' << s.p.y << '\n';
        out << s.random << '\n';
        out << s.enemies.size() << '\n';
        for (const auto& e : s.enemies)
        {
            out << e.id << ' ' << e.kind << ' ' << e.p.x << ' ' << e.p.y << ' ' << e.health << ' ' << e.maxHealth << ' '
                << e.attackTimer << ' ' << e.windup << ' ' << e.spawnTime << ' ' << e.attack << ' ' << e.aim.x << ' '
                << e.aim.y << '\n';
        }
        out << s.projectiles.size() << '\n';
        for (const auto& p : s.projectiles)
        {
            out << p.p.x << ' ' << p.p.y << ' ' << p.direction.x << ' ' << p.direction.y << ' ' << p.remaining << ' '
                << p.damage << ' ' << p.speed << ' ' << p.hostile << '\n';
        }
        out << s.drops.size() << '\n';
        for (const auto& d : s.drops)
        {
            out << d.p.x << ' ' << d.p.y << ' ' << d.kind << ' ' << d.amount << ' ' << d.attracted << '\n';
        }
        if (extended)
        {
            out << s.encounter << ' ' << s.dominantTrait << ' ' << s.allies << ' ' << s.opponents << ' '
                << s.judgementPending << ' ' << s.judgementSeen << ' ' << s.revivalUsed << ' ' << s.supportTimer << ' '
                << s.phaseTwo << '\n';
        }
    }

    inline bool Valid(float value, float low, float high)
    {
        return std::isfinite(value) && value >= low && value <= high;
    }

    inline bool ValidPoint(Point p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) < MapSize * CellSize &&
               std::abs(p.y) < MapSize * CellSize;
    }

    inline bool Read(std::istream& in, Session& s, bool extended = false)
    {
        Session loaded;
        in >> loaded.seed >> loaded.active >> loaded.completedOnce >> loaded.phase >> loaded.level >> loaded.xp >>
            loaded.weapon >> loaded.kills >> loaded.souls >> loaded.upgrades >> loaded.nextEnemyId >>
            loaded.bossState >> loaded.bossTimer >> loaded.elapsed >> loaded.health >> loaded.stamina >>
            loaded.shotTimer >> loaded.invulnerable >> loaded.spawnTimer >> loaded.magnetTimer >> loaded.p.x >>
            loaded.p.y;
        if (!in || loaded.phase < Fighting || loaded.phase > Cleared || loaded.level < 1 || loaded.level > 30 ||
            loaded.xp < 0 || loaded.xp >= NextXp(loaded) || loaded.weapon < 0 || loaded.weapon > 20 ||
            loaded.kills < 0 || loaded.souls < 0 || loaded.upgrades < 0 || loaded.nextEnemyId < 1 ||
            loaded.bossState < 0 || loaded.bossState > 3 || !Valid(loaded.bossTimer, -1, 4) ||
            !Valid(loaded.elapsed, 0, 1e8f) || !Valid(loaded.health, 0, MaxHealth(loaded)) ||
            !Valid(loaded.stamina, 0, 100) || !Valid(loaded.shotTimer, 0, 1) || !Valid(loaded.invulnerable, 0, 1) ||
            !Valid(loaded.spawnTimer, -1e8f, 5) || !Valid(loaded.magnetTimer, 0, 30) || !ValidPoint(loaded.p))
        {
            return false;
        }
        if (loaded.phase != Defeated && loaded.health <= 0)
        {
            return false;
        }
        in >> loaded.random;
        GenerateMap(loaded);
        if (!Walkable(loaded, loaded.p, 10))
        {
            return false;
        }
        size_t count = 0;
        if (!(in >> count) || count > 64)
        {
            return false;
        }
        int bosses = 0;
        for (size_t i = 0; i < count; ++i)
        {
            Enemy e;
            in >> e.id >> e.kind >> e.p.x >> e.p.y >> e.health >> e.maxHealth >> e.attackTimer >> e.windup >>
                e.spawnTime >> e.attack >> e.aim.x >> e.aim.y;
            if (!in || e.id < 1 || e.id >= loaded.nextEnemyId || e.kind < Raider || e.kind > Boss || !ValidPoint(e.p) ||
                !ValidPoint(e.aim) || !Valid(e.maxHealth, 1, 10000) || !Valid(e.health, 0, e.maxHealth) ||
                e.health <= 0 || !Valid(e.attackTimer, -1e8f, 5) || !Valid(e.windup, -1, 2) ||
                !Valid(e.spawnTime, 0, 1) || e.attack < 0 || !Walkable(loaded, e.p, e.kind == Boss ? 20.0f : 12.0f))
            {
                return false;
            }
            for (const auto& previous : loaded.enemies)
            {
                if (previous.id == e.id)
                {
                    return false;
                }
            }
            if (e.kind == Boss)
            {
                ++bosses;
            }
            loaded.enemies.push_back(e);
        }
        if (bosses != (loaded.bossState == 2 ? 1 : 0))
        {
            return false;
        }
        if (!(in >> count) || count > 4096)
        {
            return false;
        }
        for (size_t i = 0; i < count; ++i)
        {
            Projectile p;
            in >> p.p.x >> p.p.y >> p.direction.x >> p.direction.y >> p.remaining >> p.damage >> p.speed >> p.hostile;
            if (!in || !ValidPoint(p.p) || !ValidPoint(p.direction) ||
                std::abs(std::hypot(p.direction.x, p.direction.y) - 1) > 0.001 || !Valid(p.remaining, 0, 700) ||
                !Valid(p.damage, 0, 1000) || !Valid(p.speed, 1, 1000))
            {
                return false;
            }
            loaded.projectiles.push_back(p);
        }
        if (!(in >> count) || count > 100000)
        {
            return false;
        }
        for (size_t i = 0; i < count; ++i)
        {
            Drop d;
            in >> d.p.x >> d.p.y >> d.kind >> d.amount >> d.attracted;
            if (!in || !ValidPoint(d.p) || d.kind < Soul || d.kind > Magnet || d.amount < 1 || d.amount > 100000)
            {
                return false;
            }
            loaded.drops.push_back(d);
        }
        if (extended)
        {
            in >> loaded.encounter >> loaded.dominantTrait >> loaded.allies >> loaded.opponents >>
                loaded.judgementPending >> loaded.judgementSeen >> loaded.revivalUsed >> loaded.supportTimer >>
                loaded.phaseTwo;
            if (!in || loaded.encounter < 0 || loaded.encounter > 3 || loaded.dominantTrait < 0 ||
                loaded.dominantTrait > 6 || loaded.allies < 0 || loaded.allies > 8 || loaded.opponents < 0 ||
                loaded.opponents > 8 || !Valid(loaded.supportTimer, 0, 8) ||
                (loaded.judgementPending &&
                 (loaded.encounter != 3 || loaded.judgementSeen || loaded.phase != Fighting || loaded.bossState != 2)))
            {
                return false;
            }
        }
        s = loaded;
        return true;
    }
}
