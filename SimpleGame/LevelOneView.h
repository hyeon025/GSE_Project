#pragma once
#include <cwchar>
#include "GameScene.h"
#include "LevelOne.h"
#include "CombatActors.h"

class LevelOneView
{
public:
    enum Command
    {
        None,
        TogglePause,
        Resume,
        SaveGame,
        Retry,
        Leave
    };

    GameScene& scene;
    LevelOne::Session& level;
    bool paused = false;

    LevelOneView(GameScene& renderer, LevelOne::Session& session)
        : scene(renderer),
          level(session)
    {
    }

    Game::Vec Project(LevelOne::Point p) const
    {
        return scene.Project(Game::Vec(p.x, p.y));
    }

    void Ring(LevelOne::Point p, float radius, Color color, float thickness = 1)
    {
        Game::Vec previous = Project(LevelOne::Point(p.x + radius, p.y));
        for (int i = 1; i <= 64; ++i)
        {
            double a = i * 6.2831853 / 64;
            auto next = Project(LevelOne::Point(p.x + std::cos(a) * radius, p.y + std::sin(a) * radius));
            scene.r.Line((float)previous.x, (float)previous.y, (float)next.x, (float)next.y, thickness, color);
            previous = next;
        }
    }

    void Block(int x, int y)
    {
        auto p = Project({});
        unsigned variant = Game::Hash(x, y, level.seed) % 17;
        float h = (20 + variant) * IsometricView::HeightScale * scene.zoom;
        auto bounds = IsometricView::GroundHalfExtents({LevelOne::CellSize * .5, LevelOne::CellSize * .5});
        float depth = (float)bounds.y;
        float opacity = scene.SceneryOpacity(p, (float)bounds.x, h / scene.zoom + depth + 16, depth);
        scene.r.CachedMesh(
            Renderer::MeshKind::LevelBlock,
            (std::uint64_t(IsometricView::MeshRevision) << 32) | variant,
            (float)p.x,
            (float)p.y,
            scene.zoom,
            opacity,
            [this, variant]
            {
                float height = 20 + variant;
                float half = LevelOne::CellSize * .5f;
                Color top(.37f, .40f, .35f), front(.22f, .27f, .25f), side(.29f, .32f, .28f);
                GameScene mesh(scene.r, scene.world);
                mesh.zoom = 1;
                mesh.Prism(half, half, height, side, front, top);
            }
        );
        scene.Rock((float)p.x, (float)p.y - h, .7f, Game::Hash(x, y, 77), opacity);
        double half = LevelOne::CellSize * .5;
        auto edgeStart = IsometricView::Project({-half, -half, (double)(20 + variant)});
        auto edgeEnd = IsometricView::Project({half, -half, (double)(20 + variant)});
        scene.r.Line(
            (float)p.x + (float)edgeStart.x * scene.zoom,
            (float)p.y + (float)edgeStart.y * scene.zoom,
            (float)p.x + (float)edgeEnd.x * scene.zoom,
            (float)p.y + (float)edgeEnd.y * scene.zoom,
            1,
            Color(.50f, .52f, .45f, opacity)
        );
    }

    void Enemy(const LevelOne::Enemy& enemy)
    {
        auto p = Project(enemy.p);
        if (enemy.spawnTime > 0)
        {
            Ring(enemy.p, 24, Color(.78f, .42f, .33f, .7f), 2);
            return;
        }
        Art::DrawRequest imported;
        imported.assets = {
            enemy.kind == LevelOne::Boss     ? "MOB_Bear"
            : enemy.kind == LevelOne::Runner ? "MOB_Wolf"
            : enemy.kind == LevelOne::Archer ? "MOB_AshenRevenant"
                                             : "MOB_Boar"
        };
        imported.x = (float)p.x;
        imported.y = (float)p.y;
        imported.scale = scene.zoom * (enemy.kind == LevelOne::Boss ? 1.5f : 1.f);
        imported.clip = enemy.windup > 0 ? "Attack" : "Walk";
        imported.seconds = scene.world.time;
        CharacterMotion heading;
        heading.Update(level.p.x - enemy.p.x, level.p.y - enemy.p.y, 0, 0);
        imported.yaw = Art::FacingYaw(heading.facing);
        bool loaded = Art::Draw(scene.r, imported);
        if (!loaded && enemy.kind == LevelOne::Boss)
        {
            float x = (float)p.x;
            float y = (float)p.y;
            float z = scene.zoom;
            scene.zoom *= 1.65f;
            scene.Shadow(x, y, 24, 9);
            scene.Stroke(x, y, -7, -24, -9, -1, 7, Color(.24f, .25f, .24f));
            scene.Stroke(x, y, 7, -24, 9, -1, 7, Color(.19f, .22f, .22f));
            scene.Tri(
                x,
                y,
                -15,
                -52,
                -13,
                -18,
                15,
                -18,
                Color(.50f, .53f, .51f),
                Color(.28f, .31f, .31f),
                Color(.23f, .27f, .27f)
            );
            scene.Tri(
                x,
                y,
                -15,
                -52,
                15,
                -18,
                15,
                -52,
                Color(.50f, .53f, .51f),
                Color(.23f, .27f, .27f),
                Color(.40f, .44f, .43f)
            );
            scene.Oval(x, y, 0, -62, 10, 12, Color(.49f, .52f, .48f), Color(.21f, .26f, .25f));
            scene.Stroke(x, y, -7, -63, 7, -63, 2, Color(.11f, .15f, .14f));
            scene.Stroke(x, y, -5, -63, -2, -63, 1, Color(2.4f, .45f, .18f));
            scene.Stroke(x, y, 3, -63, 6, -63, 1, Color(2.4f, .45f, .18f));
            scene.Stroke(x, y, 18, -47, 22, -26, 8, Color(.38f, .43f, .42f));
            scene.Stroke(x, y, 23, -39, 30, -4, 4, Color(.65f, .68f, .63f));
            scene.Stroke(x, y, -21, -43, -21, -17, 10, Color(.32f, .37f, .36f));
            scene.zoom = z;
        }
        else if (!loaded)
        {
            scene.Person(
                Game::Vec(enemy.p.x, enemy.p.y),
                enemy.kind == LevelOne::Archer   ? Game::Matter
                : enemy.kind == LevelOne::Runner ? Game::Sense
                                                 : Game::Body,
                false,
                0,
                enemy.id
            );
        }
        float width = enemy.kind == LevelOne::Boss ? 70 : 28;
        float top = (float)p.y - (enemy.kind == LevelOne::Boss ? 128 : 62) * scene.zoom;
        scene.r.Rect((float)p.x - width * .5f, top, width, 4, Color(.10f, .12f, .11f));
        scene.r.Rect(
            (float)p.x - width * .5f,
            top,
            width * std::max(0.f, enemy.health) / enemy.maxHealth,
            4,
            Color(.73f, .33f, .27f)
        );
        if (enemy.flash > 0)
        {
            Ring(enemy.p, enemy.kind == LevelOne::Boss ? 24.f : 17.f, Color(2.f, 1.7f, 1.2f, enemy.flash * 4), 2);
        }
        if (enemy.id == level.targetId)
        {
            scene.r.Line((float)p.x - 12, top - 6, (float)p.x - 5, top - 10, 1.5f, Color(.91f, .81f, .47f));
            scene.r.Line((float)p.x + 12, top - 6, (float)p.x + 5, top - 10, 1.5f, Color(.91f, .81f, .47f));
        }
    }

    SceneGraph graph;

    enum NodeKind
    {
        Root = 1,
        Simulation,
        TerrainNode,
        Scenery,
        Characters,
        PlayerNode,
        EnemyNode,
        BlockNode,
        GrassNode,
        DropNode,
        ProjectileNode,
        EffectNode,
        MemoryNode,
        WitnessNode,
        RangeNode,
        BossWarningNode,
        WindupNode,
        LevelFlashNode
    };

    void EnsureSession()
    {
        if (m_Generation != level.generation)
        {
            graph.Clear();
            scene.playerMotion.Stop();
            m_Generation = level.generation;
        }
    }

    void Update(float dt, LevelOne::Point movement, bool sprinting)
    {
        EnsureSession();
        auto root = graph.Group({Root}).Id();
        auto& simulation = graph.GetOrCreate<CombatSimulationActor>({Simulation}, root, level);
        simulation.movement = movement;
        simulation.sprinting = sprinting;
        graph.Update(dt);
    }

    template <class Data, class Draw>
    VisualActor<Data>& Place(
        ActorKey key,
        ActorId parent,
        LevelOne::Point position,
        const Data& data,
        Draw draw,
        ActorLayer layer = ActorLayer::Objects
    )
    {
        return scene.Place(graph, key, parent, Game::Vec(position.x, position.y), data, draw, layer);
    }

    template <class Data> void AssignIds(std::vector<Data>& items)
    {
        for (auto& item : items)
        {
            if (!item.actorId)
            {
                item.actorId = ++level.nextActorId;
            }
        }
    }

    void DrawWorld()
    {
        EnsureSession();
        scene.cameraOverride = true;
        scene.camera = Game::Vec(level.p.x, level.p.y);
        AssignIds(level.drops);
        AssignIds(level.projectiles);
        AssignIds(level.effects);
        graph.BeginSync();
        auto root = graph.Group({Root}).Id();
        auto scenery = graph.Group({Scenery}, root).Id();
        auto characters = graph.Group({Characters}, root).Id();
        auto& terrain = graph.GetOrCreate<CallbackActor>({TerrainNode}, root);
        terrain.layer = ActorLayer::Background;
        terrain.draw = [this](const ActorTransform&)
        {
            scene.r.Terrain((float)level.p.x, (float)level.p.y, scene.zoom, true, (float)(level.seed % 10000));
        };
        if (level.encounter == 3)
        {
            for (size_t i = 0; i < level.memories.size(); ++i)
            {
                Place(
                    {MemoryNode, (std::int64_t)i},
                    scenery,
                    level.memories[i],
                    (unsigned)i,
                    [this](unsigned index)
                    {
                        auto p = Project({});
                        Ring({}, 28, Color(.56f, .73f, .76f, .5f));
                        scene.Rock((float)p.x, (float)p.y, .8f, index + 19);
                        scene.r.Text(
                            (float)p.x - 18,
                            (float)p.y - 38 * scene.zoom,
                            L"\uc8fd\uc74c\uc758 \uc794\ud5a5",
                            Color(.68f, .77f, .77f),
                            12
                        );
                    }
                );
            }
            int friends = 0, rivals = 0;
            for (const auto& npc : scene.world.npcs)
            {
                bool ally = npc.relationship > 70 && friends < 4;
                bool rival = npc.relationship < -70 && rivals < 4;
                if (npc.origin != 0 || (!ally && !rival))
                {
                    continue;
                }
                int slot = ally ? friends++ : rivals++;
                Place(
                    {WitnessNode, npc.id},
                    characters,
                    {ally ? -48.0 : 48.0, (slot - 1) * 48.0},
                    npc,
                    [this](const Game::Npc& data)
                    {
                        Ring(
                            {},
                            16,
                            data.relationship > 0 ? Color(.42f, .88f, .66f, .6f) : Color(.94f, .38f, .32f, .6f)
                        );
                        scene.Person(
                            {},
                            data.ability,
                            false,
                            data.archetype,
                            data.id,
                            false,
                            data.id == scene.world.story.childId
                        );
                    }
                );
            }
        }
        for (int y = 0; y < LevelOne::MapSize; ++y)
        {
            for (int x = 0; x < LevelOne::MapSize; ++x)
            {
                auto center = LevelOne::CenterOf(x, y);
                auto p = Project(center);
                if (p.x < -120 || p.x > scene.width + 120 || p.y < -100 || p.y > scene.height + 150)
                {
                    continue;
                }
                if (level.blocked[y * LevelOne::MapSize + x])
                {
                    auto& block = Place(
                        {BlockNode, x, y},
                        scenery,
                        center,
                        std::make_pair(x, y),
                        [this](const std::pair<int, int>& tile)
                        {
                            Block(tile.first, tile.second);
                        }
                    );
                    block.depthOffset =
                        IsometricView::GroundHalfExtents({LevelOne::CellSize * .5, LevelOne::CellSize * .5}).y *
                        scene.zoom;
                }
                else if ((std::abs(x - LevelOne::Center) > 2 && std::abs(y - LevelOne::Center) > 2) &&
                         Game::Hash(x, y, level.seed) % 100 < 55)
                {
                    unsigned seed = Game::Hash(x, y, level.seed);
                    center.x += (int)(seed / 100 % 21) - 10;
                    center.y += (int)(seed / 2100 % 21) - 10;
                    Place(
                        {GrassNode, x, y},
                        scenery,
                        center,
                        seed,
                        [this](unsigned seed)
                        {
                            scene.Grass(seed);
                        },
                        ActorLayer::Ground
                    );
                }
            }
        }
        auto& player = Place(
            {PlayerNode},
            characters,
            level.p,
            0,
            [this](int)
            {
                scene.Person({}, Game::Fate, true, 6, 0, true);
            }
        );
        // Blink only the player's body; child range and level-up effects remain visible.
        player.renderable = level.invulnerable <= 0 || (int)(level.invulnerable * 18) % 2 == 0;
        auto& range = Place(
            {RangeNode},
            player.Id(),
            {},
            LevelOne::Range(level),
            [this](float radius)
            {
                Ring({}, radius, Color(.73f, .79f, .63f, .12f));
            },
            ActorLayer::Ground
        );
        range.order = 1;
        if (level.bossState == 1)
        {
            auto& warning = Place(
                {BossWarningNode},
                root,
                LevelOne::CenterOf(LevelOne::Center, LevelOne::Center + 9),
                0,
                [this](int)
                {
                    Ring({}, 72, Color(1, .38f, .20f, .9f), 2);
                },
                ActorLayer::Ground
            );
            warning.order = 1;
        }
        for (const auto& enemy : level.enemies)
        {
            auto local = enemy;
            local.aim = {enemy.aim.x - enemy.p.x, enemy.aim.y - enemy.p.y};
            local.p = {};
            auto& actor = Place(
                {EnemyNode, enemy.id},
                characters,
                enemy.p,
                local,
                [this](const LevelOne::Enemy& data)
                {
                    Enemy(data);
                }
            );
            if (enemy.windup > 0)
            {
                auto& warning = Place(
                    {WindupNode, enemy.id},
                    actor.Id(),
                    {},
                    local,
                    [this](const LevelOne::Enemy& data)
                    {
                        DrawWindup(data);
                    },
                    ActorLayer::Ground
                );
                warning.order = 1;
            }
        }
        for (const auto& drop : level.drops)
        {
            auto local = drop;
            local.p = {};
            auto& actor = Place(
                {DropNode, drop.actorId},
                root,
                drop.p,
                local,
                [this](const LevelOne::Drop& data)
                {
                    DrawDrop(data);
                },
                ActorLayer::Ground
            );
            actor.order = 2;
        }
        for (const auto& shot : level.projectiles)
        {
            auto local = shot;
            local.p = {};
            Place(
                {ProjectileNode, shot.actorId},
                root,
                shot.p,
                local,
                [this](const LevelOne::Projectile& data)
                {
                    DrawProjectile(data);
                },
                ActorLayer::Effects
            );
        }
        for (const auto& effect : level.effects)
        {
            auto& actor = Place(
                {EffectNode, effect.actorId},
                root,
                effect.p,
                effect,
                [this](const LevelOne::HitEffect& data)
                {
                    Ring(
                        {},
                        (.4f - data.time) * 90,
                        data.healing ? Color(.5f, 1.2f, .6f, data.time * 2) : Color(1.8f, .9f, .5f, data.time * 2),
                        2
                    );
                },
                ActorLayer::Effects
            );
            actor.order = 1;
        }
        if (level.levelFlash > 0)
        {
            auto& flash = Place(
                {LevelFlashNode},
                player.Id(),
                {},
                level.levelFlash,
                [this](float time)
                {
                    Ring({}, (1.2f - time) * 100, Color(.62f, 1.3f, 1.1f, time * .4f), 2);
                },
                ActorLayer::Effects
            );
            flash.order = 2;
        }
        graph.EndSync();
        scene.RenderGraph(graph);
    }

    void DrawHud(int mouseX, int mouseY)
    {
        scale = std::min(1.f, std::min(scene.width / 960.f, scene.height / 640.f));
        width = scene.width / scale;
        height = scene.height / scale;
        mx = mouseX / scale;
        my = mouseY / scale;
        buttons.clear();
        Rect(0, 0, width, 92, Color(.05f, .075f, .067f, .96f));
        Text(24, 12, LevelOne::AreaName(level), Color(.91f, .91f, .85f), 20);
        Text(
            24,
            45,
            L"\uc0dd\uba85 " + std::to_wstring((int)level.health) + L" / " +
                std::to_wstring((int)LevelOne::MaxHealth(level)),
            Color(.81f, .85f, .79f),
            14
        );
        Bar(24, 72, 260, level.health / LevelOne::MaxHealth(level), Color(.68f, .32f, .27f));
        Bar(24, 82, 260, level.stamina / 100, Color(.38f, .61f, .47f));
        Text(324, 16, L"Lv. " + std::to_wstring(level.level), Color(.60f, .85f, .73f), 22);
        Text(
            407,
            19,
            level.level == 30 ? L"\ucd5c\uace0 \ub808\ubca8"
                              : L"\uacbd\ud5d8\uce58 " + std::to_wstring(level.xp) + L" / " +
                                    std::to_wstring(LevelOne::NextXp(level)),
            Color(.70f, .78f, .73f),
            14
        );
        Bar(324, 52, 248, level.level == 30 ? 1.f : (float)level.xp / LevelOne::NextXp(level), Color(.42f, .75f, .64f));
        Text(
            324,
            66,
            L"\ucc98\uce58 " + std::to_wstring(level.kills) + L"    \uc601\ud63c\uc11d " + std::to_wstring(level.souls),
            Color(.75f, .79f, .72f),
            13
        );
        Text(
            width - 230,
            19,
            L"\ud68c\uc0c9 \uc11d\uad81 +" + std::to_wstring(level.weapon),
            Color(.88f, .79f, .55f),
            16
        );
        PauseButton(width - 60, 49);
        const LevelOne::Enemy* boss = nullptr;
        for (const auto& enemy : level.enemies)
        {
            if (enemy.kind == LevelOne::Boss)
            {
                boss = &enemy;
            }
        }
        if (boss)
        {
            float bw = std::min(540.f, width - 360);
            float bx = (width - bw) * .5f;
            Rect(bx - 14, 108, bw + 28, 62, Color(.09f, .075f, .065f, .95f));
            Text(bx, 115, LevelOne::BossName(level), Color(.92f, .73f, .56f), 18);
            Bar(bx, 151, bw, boss->health / boss->maxHealth, Color(.74f, .32f, .24f));
        }
        else if (level.encounter == 0)
        {
            Text(
                24,
                112,
                level.bossState == 1 ? L"\ubcf4\uc2a4 \ucd9c\ud604\uae4c\uc9c0 " +
                                           std::to_wstring((int)std::ceil(level.bossTimer)) + L"\ucd08"
                : level.phase == LevelOne::Cleared
                    ? L"\uacbd\uc791\uc9c0\uc758 \ubd88\ube5b\uc774 \ub3cc\uc544\uc654\ub2e4"
                    : L"\ud30c\ubc0d \ubaa9\ud45c",
                Color(.86f, .81f, .64f),
                17
            );
            if (level.bossState == 0)
            {
                Text(
                    24,
                    143,
                    L"\uc601\ud63c\uc11d " + std::to_wstring(std::min(LevelOne::SoulGoal, level.souls)) + L" / 12",
                    Color(.70f, .83f, .74f),
                    14
                );
                Text(
                    24,
                    167,
                    L"\ubb34\uae30 \uac15\ud654 " + std::to_wstring(std::min(1, level.upgrades)) + L" / 1",
                    Color(.86f, .76f, .52f),
                    14
                );
                Text(
                    24,
                    191,
                    L"\ub808\ubca8 " + std::to_wstring(std::min(4, level.level)) + L" / 4",
                    Color(.70f, .83f, .74f),
                    14
                );
                Text(
                    24,
                    215,
                    L"\uc801 \ucc98\uce58 " + std::to_wstring(std::min(LevelOne::KillGoal, level.kills)) + L" / 24",
                    Color(.80f, .74f, .65f),
                    14
                );
            }
        }
        if (level.encounter == 1 && level.phase == LevelOne::Fighting)
        {
            Text(24, 112, L"\ubcc0\ubc29 \ub9c8\uc744\uc744 \uc9c0\ucf1c\ub77c", Color(.91f, .76f, .62f), 18);
            Text(
                24,
                144,
                L"\ud53c\ub09c\ubbfc\ub4e4\uc774 \uc131\ubb38 \ub4a4\ub85c \ub2ec\uc544\ub098\uace0 \uc788\ub2e4.",
                Color(.76f, .80f, .72f),
                14
            );
        }
        if (level.encounter == 3)
        {
            Text(
                24,
                184,
                L"\ud568\uaed8 \uc120 \uc774\ub4e4 " + std::to_wstring(level.allies),
                Color(.61f, .83f, .73f),
                14
            );
            Text(
                24,
                208,
                L"\ub4f1\uc744 \ub3cc\ub9b0 \uc774\ub4e4 " + std::to_wstring(level.opponents),
                Color(.87f, .55f, .48f),
                14
            );
        }
        MiniMap(width - 186, 112);
        Rect(0, height - 64, width, 64, Color(.05f, .075f, .067f, .96f));
        Text(
            24,
            height - 49,
            L"\ud53c\ud574\ub7c9 " + std::to_wstring((int)LevelOne::Damage(level)) + L"     \uc0ac\uac70\ub9ac " +
                std::to_wstring((int)LevelOne::Range(level)),
            Color(.85f, .83f, .71f),
            15
        );
        wchar_t cadence[64] = {};
        swprintf_s(cadence, L"\ubc1c\uc0ac \uac04\uaca9 %.2f\ucd08", LevelOne::Cooldown(level));
        Text(310, height - 49, cadence, Color(.70f, .81f, .75f), 15);
        Bar(310, height - 20, 164, 1 - level.shotTimer / LevelOne::Cooldown(level), Color(.69f, .67f, .42f));
        Text(
            522,
            height - 49,
            level.magnetTimer > 0
                ? L"\ud761\uc778\uc11d " + std::to_wstring((int)std::ceil(level.magnetTimer)) + L"\ucd08"
                : L"\uc2b5\ub4dd \ubc18\uacbd " + std::to_wstring((int)LevelOne::PickupRadius(level)),
            Color(.68f, .78f, .89f),
            15
        );
        Text(
            width - 214,
            height - 27,
            L"\uacbd\uc791\uc9c0 \uae30\ub85d " + std::to_wstring(level.seed % 100000),
            Color(.50f, .60f, .54f),
            12
        );
        if (level.noticeTimer > 0 && !paused && level.phase == LevelOne::Fighting)
        {
            Rect((width - 600) * .5f, height - 120, 600, 42, Color(.07f, .12f, .09f, .96f));
            Text((width - 600) * .5f + 16, height - 110, level.notice, Color(.84f, .90f, .80f), 15);
        }
        if (paused || level.phase != LevelOne::Fighting || level.judgementPending)
        {
            buttons.clear();
            Rect(0, 0, width, height, Color(.02f, .04f, .03f, .76f));
            float x = (width - 520) * .5f;
            float y = (height - 326) * .5f;
            Rect(x, y, 520, 326, Color(.08f, .11f, .09f, .99f));
            Text(
                x + 26,
                y + 24,
                level.judgementPending ? L"\uc5d0\ub179\uc758 \ub9c8\uc9c0\ub9c9 \uc9c8\ubb38"
                : level.phase == LevelOne::Cleared && level.encounter ? L"\ub300\uba74\uc744 \ub9c8\ucce4\ub2e4"
                : level.phase == LevelOne::Cleared                    ? L"\ub808\ubca8 1 \uc644\ub8cc"
                : level.phase == LevelOne::Defeated                   ? L"\uc774 \uc0b6\uc774 \ub0a8\uae34 \uac83"
                                                                      : L"\uc7a0\uc2dc \uc228\uc744 \uace0\ub978\ub2e4",
                Color(.91f, .89f, .78f),
                25
            );
            Text(
                x + 26,
                y + 74,
                L"Lv. " + std::to_wstring(level.level) + L"   \u00b7   \ucc98\uce58 " + std::to_wstring(level.kills) +
                    L"   \u00b7   \ubb34\uae30 +" + std::to_wstring(level.weapon),
                Color(.71f, .81f, .71f),
                16
            );
            Text(
                x + 26,
                y + 110,
                level.judgementPending
                    ? L"\ub0b4\uac00 \uc0ac\ub77c\uc9c0\uba74 \uc8fd\uc74c\uc740 \ub2e4\uc2dc \uc8fd\uc74c\uc774 \ub41c\ub2e4."
                : level.phase == LevelOne::Defeated
                    ? (scene.world.story.ending == Story::Destroy
                           ? L"\uc774 \uc8fd\uc74c\uc5d0\uc11c \uacc4\uc2b9\uc790\ub294 \ud0dc\uc5b4\ub098\uc9c0 \uc54a\uc558\ub2e4."
                           : L"\ub2f9\uc2e0\uc758 \uc0b6\uc740 \ub05d\ub0ac\ub2e4. \uadf8\ub7ec\ub098 \ub0a8\uae34 \ubc29\ud5a5\uc740 \ub05d\ub098\uc9c0 \uc54a\uc558\ub2e4."
                      )
                : level.phase == LevelOne::Cleared
                    ? L"\ubcf4\uc0c1\uc744 \ud68c\uc218\ud588\ub2e4. \uc774\uc81c \ub354 \uba3c \uae38\ub85c \ub098\uc544\uac08 \uc218 \uc788\ub2e4."
                    : L"\uacbd\uc791\uc9c0\uc758 \uc2dc\uac04\ub3c4 \uc7a0\uc2dc \uba48\ucd98\ub2e4.",
                Color(.65f, .73f, .65f),
                15
            );
            if (level.judgementPending)
            {
                Text(
                    x + 26,
                    y + 133,
                    L"\ub2e4\uc2dc\ub294 \uadf8 \uc8fd\uc74c\uc5d0\uc11c \ub204\uad70\uac00 \ud0dc\uc5b4\ub098\uc9c0 \uc54a\ub294\ub2e4. \uadf8\ub798\ub3c4?",
                    Color(.86f, .74f, .62f),
                    14
                );
            }
            bool leaveStory = level.encounter && (level.phase == LevelOne::Cleared ||
                                                  (level.encounter == 1 && level.phase == LevelOne::Defeated));
            Button(
                x + 26,
                y + 157,
                468,
                42,
                level.judgementPending
                    ? L"\uadf8\ub798\ub3c4 \uc6b0\ub9ac\uc758 \uc120\ud0dd\uc73c\ub85c \ub05d\ub0b4\uaca0\ub2e4"
                : leaveStory ? L"\uc5ec\uc815\uc73c\ub85c \ub3cc\uc544\uac04\ub2e4"
                : level.encounter && level.phase == LevelOne::Defeated ? L"\ub2e4\uc2dc \ub300\uba74\ud55c\ub2e4"
                : level.phase == LevelOne::Fighting                    ? L"\uacc4\uc18d\ud55c\ub2e4"
                                                    : L"\uc0c8 \uacbd\uc791\uc9c0\uc5d0 \ub3c4\uc804\ud55c\ub2e4",
                leaveStory                          ? Leave
                : level.phase == LevelOne::Fighting ? Resume
                                                    : Retry
            );
            Button(x + 26, y + 209, 226, 42, L"\uc800\uc7a5", SaveGame);
            Button(x + 268, y + 209, 226, 42, L"\uc138\uacc4\ub85c \ub3cc\uc544\uac04\ub2e4", Leave);
            if (level.noticeTimer > 0)
            {
                Text(x + 26, y + 275, level.notice.substr(0, 30), Color(.78f, .83f, .73f), 13);
            }
        }
        scene.r.Flush();
    }

    Command Hit(int x, int y) const
    {
        float ux = x / scale, uy = y / scale;
        for (auto i = buttons.rbegin(); i != buttons.rend(); ++i)
        {
            if (ux >= i->x && ux < i->x + i->w && uy >= i->y && uy < i->y + i->h)
            {
                return i->command;
            }
        }
        return None;
    }

private:
    std::uint64_t m_Generation = ~std::uint64_t(0);

    void DrawDrop(const LevelOne::Drop& drop)
    {
        auto p = Project(drop.p);
        float x = (float)p.x, y = (float)p.y - 6 * scene.zoom;
        Color color = drop.kind == LevelOne::Soul      ? Color(.49f, 1.35f, 1.1f)
                      : drop.kind == LevelOne::Weapon  ? Color(1.4f, 1.05f, .42f)
                      : drop.kind == LevelOne::Healing ? Color(.92f, .31f, .29f)
                                                       : Color(.66f, .70f, 1.4f);
        scene.Shadow(x, y + 7 * scene.zoom, 7, 3);
        if (drop.kind == LevelOne::Healing)
        {
            scene.Stroke(x, y, -4, 0, 4, 0, 3, color);
            scene.Stroke(x, y, 0, -4, 0, 4, 3, color);
        }
        else if (drop.kind == LevelOne::Weapon)
        {
            scene.Stroke(x, y, -4, 4, 4, -6, 2.8f, color);
            scene.Stroke(x, y, -5, -1, 1, 3, 2, color);
        }
        else if (drop.kind == LevelOne::Magnet)
        {
            scene.Stroke(x, y, -4, -4, -4, 3, 2, color);
            scene.Stroke(x, y, 4, -4, 4, 3, 2, color);
            scene.Stroke(x, y, -4, 3, 4, 3, 2, color);
        }
        else
        {
            scene.Tri(x, y, 0, -6, -4, 0, 0, 5, color, scene.Shade(color, .5f), scene.Shade(color, .6f));
            scene.Tri(x, y, 0, -6, 0, 5, 4, 0, color, scene.Shade(color, .6f), color);
        }
    }

    void DrawWindup(const LevelOne::Enemy& enemy)
    {
        if (enemy.kind == LevelOne::Boss && enemy.attack % 2 == 0)
        {
            Ring(enemy.aim, 90, Color(.94f, .25f, .17f, .85f), 2);
            Ring(enemy.aim, 90 * (1 - enemy.windup / 1.2f), Color(.94f, .42f, .23f, .55f));
        }
        else
        {
            Ring(enemy.p, enemy.kind == LevelOne::Boss ? 70.f : 25.f, Color(.86f, .38f, .24f, .8f), 2);
            if (enemy.kind == LevelOne::Archer)
            {
                auto from = Project(enemy.p);
                auto to = Project(enemy.aim);
                scene.r.Line(
                    (float)from.x,
                    (float)from.y - 24 * scene.zoom,
                    (float)to.x,
                    (float)to.y - 24 * scene.zoom,
                    1,
                    Color(.88f, .35f, .26f, .45f)
                );
            }
        }
    }

    void DrawProjectile(const LevelOne::Projectile& shot)
    {
        auto p = Project(shot.p);
        auto tail = Project(LevelOne::Point(shot.p.x - shot.direction.x * 12, shot.p.y - shot.direction.y * 12));
        Color c = shot.hostile ? Color(2.5f, .45f, .20f) : Color(2.2f, 1.8f, .75f);
        scene.r.Line(
            (float)tail.x,
            (float)tail.y - 24 * scene.zoom,
            (float)p.x,
            (float)p.y - 24 * scene.zoom,
            2 * scene.zoom,
            c
        );
    }

    struct Region
    {
        float x, y, w, h;
        Command command;
    };

    std::vector<Region> buttons;
    float scale = 1, width = 1440, height = 900, mx = 0, my = 0;

    void Rect(float x, float y, float w, float h, Color color)
    {
        scene.r.Rect(x * scale, y * scale, w * scale, h * scale, color);
    }

    void Text(float x, float y, const std::wstring& text, Color color, int size)
    {
        scene.r.Text(x * scale, y * scale, text, color, std::max(8, (int)(size * scale)));
    }

    void Bar(float x, float y, float w, float fill, Color color)
    {
        Rect(x, y, w, 5, Color(.12f, .16f, .13f));
        Rect(x, y, w * std::max(0.f, std::min(1.f, fill)), 5, color);
    }

    void Button(float x, float y, float w, float h, const std::wstring& label, Command command)
    {
        bool hover = mx >= x && mx < x + w && my >= y && my < y + h;
        Rect(x, y, w, h, hover ? Color(.29f, .36f, .29f) : Color(.18f, .24f, .20f));
        Text(x + 14, y + 10, label, Color(.88f, .89f, .79f), 16);
        buttons.push_back({x, y, w, h, command});
    }

    void PauseButton(float x, float y)
    {
        Rect(x, y, 32, 30, Color(.19f, .25f, .20f));
        Rect(x + 9, y + 7, 4, 16, Color(.81f, .85f, .78f));
        Rect(x + 19, y + 7, 4, 16, Color(.81f, .85f, .78f));
        buttons.push_back({x, y, 32, 30, TogglePause});
    }

    void MiniMap(float x, float y)
    {
        Rect(x - 7, y - 7, 169, 169, Color(.06f, .10f, .08f, .94f));
        auto mapPoint = [&](double gx, double gy)
        {
            auto p = IsometricView::Project({gx, gy, 0});
            return IsometricView::Point{x + 77.5 + p.x * 2.4, y + 77.5 + p.y * 2.4};
        };
        for (int cy = 0; cy < LevelOne::MapSize; ++cy)
        {
            for (int cx = 0; cx < LevelOne::MapSize; ++cx)
            {
                double gx = cx - LevelOne::Center, gy = cy - LevelOne::Center;
                auto a = mapPoint(gx - .5, gy - .5), b = mapPoint(gx + .5, gy - .5);
                auto c = mapPoint(gx + .5, gy + .5), d = mapPoint(gx - .5, gy + .5);
                Color color =
                    level.blocked[cy * LevelOne::MapSize + cx] ? Color(.30f, .32f, .27f) : Color(.15f, .23f, .17f);
                scene.r.Triangle(
                    (float)a.x * scale,
                    (float)a.y * scale,
                    (float)b.x * scale,
                    (float)b.y * scale,
                    (float)c.x * scale,
                    (float)c.y * scale,
                    color,
                    color,
                    color
                );
                scene.r.Triangle(
                    (float)a.x * scale,
                    (float)a.y * scale,
                    (float)c.x * scale,
                    (float)c.y * scale,
                    (float)d.x * scale,
                    (float)d.y * scale,
                    color,
                    color,
                    color
                );
            }
        }
        auto marker = [&](LevelOne::Point p, Color c, float size)
        {
            auto point = mapPoint(p.x / LevelOne::CellSize, p.y / LevelOne::CellSize);
            Rect((float)point.x - size * .5f, (float)point.y - size * .5f, size, size, c);
        };
        for (const auto& e : level.enemies)
        {
            marker(
                e.p,
                e.kind == LevelOne::Boss ? Color(1, .57f, .23f) : Color(.72f, .33f, .27f),
                e.kind == LevelOne::Boss ? 6.f : 3.f
            );
        }
        if (level.bossState == 1)
        {
            marker(LevelOne::CenterOf(LevelOne::Center, LevelOne::Center + 9), Color(1, .65f, .3f), 7);
        }
        marker(level.p, Color(.88f, .95f, .80f), 5);
    }
};
