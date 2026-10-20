#pragma once
#include "GameWorld.h"
#include "SceneGraph.h"
#include "WorldActors.h"
#include "CharacterMesh.h"
#include "WorldSimulation.h"
#include "IsometricView.h"
#include "ArtPresentation.h"

class GameScene
{
public:
    Renderer& r;
    Game::World& world;
    int width = 1440, height = 900;
    float zoom = IsometricView::DefaultZoom;
    CharacterMotion playerMotion;
    bool cameraOverride = false;
    Game::Vec camera;

    GameScene(Renderer& renderer, Game::World& w)
        : r(renderer),
          world(w)
    {
    }

    SceneGraph graph;
    SceneGraph interfaceGraph;
    SceneGraph simulationGraph;

    enum NodeKind
    {
        Root = 1,
        TerrainNode,
        Scenery,
        Characters,
        PlayerNode,
        NpcNode,
        ObstacleNode,
        GrassNode,
        SiteNode,
        TargetNode,
        HudNode,
        VillageNode,
        BuildingNode,
        WorksiteNode,
        SimulationNode,
        HouseNode
    };

    Game::Vec Project(Game::Vec p) const
    {
        float projectionZoom = zoom;
        double elevation = 0;
        if (m_ActorTransform)
        {
            auto point = ActorTransform::Compose(*m_ActorTransform, {p.x, p.y});
            p = Game::Vec(point.x, point.y);
            elevation = point.height;
            projectionZoom = m_ProjectionZoom;
        }
        const Game::Vec& origin = cameraOverride ? camera : world.player.p;
        auto projected = IsometricView::Project({p.x - origin.x, p.y - origin.y, elevation});
        return Game::Vec(
            width * .5 + projected.x * projectionZoom,
            height * IsometricView::AnchorY + projected.y * projectionZoom
        );
    }

    float SceneryOpacity(Game::Vec ground, float halfWidth, float above, float below = 0) const
    {
        return IsometricView::OccluderOpacity(
            {(ground.x - width * .5) / zoom, (ground.y - height * IsometricView::AnchorY) / zoom},
            halfWidth,
            above,
            below
        );
    }

    void Surface(IsometricView::Vertex a, IsometricView::Vertex b, IsometricView::Vertex c, Color color)
    {
        auto pa = IsometricView::Project(a), pb = IsometricView::Project(b), pc = IsometricView::Project(c);
        Tri(0, 0, (float)pa.x, (float)pa.y, (float)pb.x, (float)pb.y, (float)pc.x, (float)pc.y, color, color, color);
    }

    void Surface(
        IsometricView::Vertex a,
        IsometricView::Vertex b,
        IsometricView::Vertex c,
        IsometricView::Vertex d,
        Color color
    )
    {
        Surface(a, b, c, color);
        Surface(a, c, d, color);
    }

    void Edge(IsometricView::Vertex a, IsometricView::Vertex b, float thickness, Color color)
    {
        auto pa = IsometricView::Project(a), pb = IsometricView::Project(b);
        Stroke(0, 0, (float)pa.x, (float)pa.y, (float)pb.x, (float)pb.y, thickness, color);
    }

    void Prism(float halfX, float halfY, float height, Color left, Color right, Color top)
    {
        Surface({-halfX, halfY, 0}, {-halfX, -halfY, 0}, {-halfX, -halfY, height}, {-halfX, halfY, height}, left);
        Surface({-halfX, -halfY, 0}, {halfX, -halfY, 0}, {halfX, -halfY, height}, {-halfX, -halfY, height}, right);
        Surface(
            {-halfX, -halfY, height},
            {halfX, -halfY, height},
            {halfX, halfY, height},
            {-halfX, halfY, height},
            top
        );
    }

    template <class Draw> void DrawAt(const ActorTransform& transform, Draw draw)
    {
        struct Scope
        {
            GameScene& scene;
            float zoom, projectionZoom;
            const ActorTransform* transform;

            ~Scope()
            {
                scene.zoom = zoom;
                scene.m_ProjectionZoom = projectionZoom;
                scene.m_ActorTransform = transform;
            }
        } scope{*this, zoom, m_ProjectionZoom, m_ActorTransform};

        m_ProjectionZoom = zoom;
        m_ActorTransform = &transform;
        zoom *= (float)transform.scale;
        draw();
    }

    template <class Data, class Draw>
    VisualActor<Data>& Place(
        SceneGraph& nodes,
        ActorKey key,
        ActorId parent,
        Game::Vec position,
        const Data& data,
        Draw draw,
        ActorLayer layer = ActorLayer::Objects
    )
    {
        auto& actor = nodes.GetOrCreate<VisualActor<Data>>(
            key,
            parent,
            [this, draw](const Data& snapshot, const ActorTransform& transform)
            {
                DrawAt(
                    transform,
                    [&]
                    {
                        draw(snapshot);
                    }
                );
            }
        );
        actor.data = data;
        auto pose = actor.LocalTransform();
        pose.x = position.x;
        pose.y = position.y;
        actor.SetLocalTransform(pose);
        actor.layer = layer;
        actor.streamed = true;
        return actor;
    }

    void RenderGraph(SceneGraph& nodes)
    {
        nodes.Render(
            [this](const ActorTransform& transform)
            {
                // Sort at the ground contact point even when a billboard is elevated.
                return Project(Game::Vec(transform.x, transform.y)).y;
            }
        );
        r.Flush();
    }

    void DrawOverlay(const std::function<void()>& draw)
    {
        auto& hud = interfaceGraph.GetOrCreate<CallbackActor>({HudNode}, 0);
        hud.layer = ActorLayer::Interface;
        hud.draw = [draw](const ActorTransform&)
        {
            draw();
        };
        interfaceGraph.Render();
        r.Flush();
    }

    WorldPlayerActor& SyncCharacters()
    {
        for (auto& npc : world.npcs)
        {
            if (!npc.artParts.empty())
            {
                continue;
            }
            auto parts = Art::Assets().Recipe((unsigned)npc.id);
            if (!parts.empty())
            {
                npc.artParts = std::move(parts);
                npc.artCatalogVersion = 1;
                world.dirty = true;
            }
        }
        m_InspectedNpc = 0;
        double nearest = 90;
        for (const auto& n : world.npcs)
        {
            double distance = Game::Distance(n.p, world.player.p);
            if (n.life.alive && distance < nearest)
            {
                nearest = distance;
                m_InspectedNpc = n.id;
            }
        }
        auto root = graph.Group({Root}).Id();
        auto parent = graph.Group({Characters}, root).Id();
        auto& player = graph.GetOrCreate<WorldPlayerActor>(
            {PlayerNode},
            parent,
            world,
            playerMotion,
            [this](const Game::Player& p, const ActorTransform& transform)
            {
                DrawAt(
                    transform,
                    [&]
                    {
                        Person({}, p.ability, true);
                    }
                );
            }
        );
        player.data = world.player;
        auto pose = player.LocalTransform();
        pose.x = world.player.p.x;
        pose.y = world.player.p.y;
        player.SetLocalTransform(pose);
        for (size_t i = 0; i < world.npcs.size(); ++i)
        {
            const auto& n = world.npcs[i];
            if (!n.life.alive || Game::Distance(n.p, world.player.p) >= 1400)
            {
                continue;
            }
            auto& npc = graph.GetOrCreate<WorldNpcActor>(
                {NpcNode, n.id},
                parent,
                world,
                [this](const Game::Npc& data, const ActorTransform& transform)
                {
                    DrawAt(
                        transform,
                        [&]
                        {
                            Person(
                                {},
                                data.ability,
                                false,
                                data.archetype,
                                data.id,
                                data.life.job == Society::Hunter,
                                data.id == world.story.childId,
                                &data.motion
                            );
                            auto p = Project({});
                            Color color = data.life.target ? Color(.88f, .35f, .29f) : Color(.65f, .80f, .65f);
                            bool inspected = data.id == m_InspectedNpc;
                            if (inspected || data.life.health < data.life.maxHealth)
                            {
                                r.Rect(
                                    (float)p.x - 17 * zoom,
                                    (float)p.y - 72 * zoom,
                                    34 * zoom,
                                    3 * zoom,
                                    Color(.08f, .09f, .08f)
                                );
                                r.Rect(
                                    (float)p.x - 17 * zoom,
                                    (float)p.y - 72 * zoom,
                                    34 * zoom * data.life.health / data.life.maxHealth,
                                    3 * zoom,
                                    color
                                );
                            }
                            if (inspected)
                            {
                                r.Text((float)p.x - 27 * zoom, (float)p.y - 108 * zoom, data.Name(), color, 12);
                                r.Text(
                                    (float)p.x - 42 * zoom,
                                    (float)p.y - 91 * zoom,
                                    std::wstring(Society::VillageName(data.life.village)) + L" / " +
                                        Society::JobName(data.life.job),
                                    color,
                                    12
                                );
                            }
                        }
                    );
                }
            );
            npc.data = n;
            npc.visible = true;
            npc.modelIndex = i;
            pose = npc.LocalTransform();
            pose.x = n.p.x;
            pose.y = n.p.y;
            npc.SetLocalTransform(pose);
            npc.streamed = true;
        }
        return player;
    }

    void Update(float dt, Game::Vec movement, bool sprinting)
    {
        auto& player = SyncCharacters();
        player.movement = movement;
        player.sprinting = sprinting;
        graph.Update(dt);
    }

    void DrawVillageBuilding(int kind)
    {
        auto p = Project({});
        const char* ids[] = {"SM_ENV_Building_01_Cottage", "SM_ENV_Building_02_Smithy", "SM_ENV_Building_03_Market"};
        if (Art::Environment(r, ids[kind % 3], (float)p.x, (float)p.y, zoom, 8, SceneryOpacity(p, 56, 130, 32)))
        {
            return;
        }
        r.CachedMesh(
            Renderer::MeshKind::VillageBuilding,
            (std::uint64_t(IsometricView::MeshRevision) << 32) | (kind + 1),
            (float)p.x,
            (float)p.y,
            zoom,
            SceneryOpacity(p, 56, 130, (float)IsometricView::GroundHalfExtents({25, 23}).y),
            [this, kind]
            {
                GameScene mesh(r, world);
                mesh.zoom = 1;
                Color stone(.48f, .49f, .44f), shade(.28f, .33f, .32f), mortar(.20f, .24f, .23f);
                Color roof = kind == 2 ? Color(.29f, .43f, .38f) : Color(.37f, .32f, .31f);
                mesh.Oval(0, 0, 18, 9, 65, 32, Color(.02f, .03f, .03f, .42f), Color(.02f, .03f, .03f, 0));
                // The foundation matches the existing +/-25 by +/-23 collision footprint.
                mesh.Prism(25, 23, 72, stone, shade, stone);
                for (int z = 12; z < 72; z += 12)
                {
                    mesh.Edge({-25, -23, (double)z}, {25, -23, (double)z}, .65f, mortar);
                    mesh.Edge({-25, -23, (double)z}, {-25, 23, (double)z}, .65f, mesh.Shade(stone, .7f));
                    for (int x = -20 + (z / 12 % 2) * 7; x < 25; x += 14)
                    {
                        mesh.Edge({(double)x, -23, (double)z - 12}, {(double)x, -23, (double)z}, .55f, mortar);
                    }
                }
                Color timber(.16f, .18f, .16f), glow(.83f, .67f, .36f);
                mesh.Surface({-9, -23, 0}, {9, -23, 0}, {9, -23, 54}, {-9, -23, 54}, timber);
                mesh.Edge({-9, -23, 54}, {9, -23, 54}, 2, mesh.Shade(stone, 1.2f));
                mesh.Edge({-9, -23, 0}, {-9, -23, 54}, 1.5f, stone);
                mesh.Edge({9, -23, 0}, {9, -23, 54}, 1.5f, stone);
                mesh.Edge({0, -23, 1}, {0, -23, 53}, .8f, mesh.Shade(timber, .6f));
                mesh.Surface({-25, -10, 33}, {-25, 5, 33}, {-25, 5, 51}, {-25, -10, 51}, glow);
                mesh.Edge({-25, -2.5, 33}, {-25, -2.5, 51}, 1.7f, timber);
                mesh.Edge({-25, -10, 42}, {-25, 5, 42}, 1.7f, timber);
                mesh.Surface({-25, -23, 72}, {-25, 23, 72}, {-25, 0, 95}, mesh.Shade(stone, .9f));
                mesh.Surface({-29, 27, 68}, {29, 27, 68}, {29, 0, 95}, {-29, 0, 95}, mesh.Shade(roof, 1.15f));
                mesh.Surface({-29, -27, 68}, {29, -27, 68}, {29, 0, 95}, {-29, 0, 95}, mesh.Shade(roof, .78f));
                for (int x = -22; x < 29; x += 8)
                {
                    mesh.Edge({(double)x, -27, 68}, {(double)x, 0, 95}, .6f, mesh.Shade(roof, .58f));
                    mesh.Edge({(double)x, 27, 68}, {(double)x, 0, 95}, .6f, mesh.Shade(roof, .85f));
                }
                for (int y = 7; y < 27; y += 7)
                {
                    mesh.Edge({-29, (double)-y, (double)95 - y}, {29, (double)-y, (double)95 - y}, .65f, roof);
                    mesh.Edge(
                        {-29, (double)y, (double)95 - y},
                        {29, (double)y, (double)95 - y},
                        .65f,
                        mesh.Shade(roof, 1.3f)
                    );
                }
                mesh.Edge({-29, 0, 95}, {29, 0, 95}, 1.7f, mesh.Shade(roof, 1.35f));
                mesh.Edge({-29, -27, 68}, {29, -27, 68}, 2, timber);
            }
        );
    }

    void SyncVillages(ActorId scenery)
    {
        for (int village = 0; village < Society::VillageCount; ++village)
        {
            auto center = Game::VillageCenter(village);
            if (Game::Distance(center, world.player.p) > 2400)
            {
                continue;
            }
            Place(
                graph,
                {VillageNode, village},
                scenery,
                center,
                village,
                [this](int v)
                {
                    auto p = Project({});
                    r.Ellipse(
                        (float)p.x,
                        (float)p.y,
                        (float)Society::VillageRadius * zoom,
                        (float)Society::VillageRadius * IsometricView::GroundDepth * zoom,
                        Color(.34f, .36f, .31f, .78f),
                        Color(.25f, .31f, .27f, 0)
                    );
                    auto hunt = Project(Game::Vec(190, -190));
                    auto shrine = Project(Game::Vec(-135, 135));
                    r.Line(
                        (float)hunt.x,
                        (float)hunt.y,
                        (float)shrine.x,
                        (float)shrine.y,
                        18 * zoom,
                        Color(.38f, .38f, .32f, .65f)
                    );
                    r.Text(
                        (float)p.x - 38 * zoom,
                        (float)p.y + 35 * zoom,
                        Society::VillageName(v),
                        Color(.85f, .83f, .70f),
                        15
                    );
                },
                ActorLayer::Ground
            );
            for (int lot = 0; lot < Society::HousingCapacity; ++lot)
            {
                auto position = Game::HousePosition(village, lot);
                auto screen = Project(position);
                if (screen.x < -120 || screen.x > width + 120 || screen.y < -100 || screen.y > height + 160)
                {
                    continue;
                }
                auto& house = Place(
                    graph,
                    {HouseNode, village, lot},
                    scenery,
                    position,
                    lot,
                    [this, village](int plot)
                    {
                        DrawVillageBuilding(plot % 3);
                        if (Game::Distance(world.player.p, Game::HouseEntrance(village, plot)) < 90)
                        {
                            for (const auto& owner : world.npcs)
                            {
                                if (!owner.life.alive || owner.life.village != village || owner.life.houseLot != plot)
                                {
                                    continue;
                                }
                                auto p = Project({});
                                r.Text(
                                    (float)p.x - 35 * zoom,
                                    (float)p.y - 120 * zoom,
                                    owner.Name(),
                                    Color(.9f, .86f, .7f),
                                    12
                                );
                                break;
                            }
                        }
                    }
                );
                house.depthOffset = IsometricView::GroundHalfExtents({25, 23}).y * zoom;
            }
            for (int building = 0; building < 3; ++building)
            {
                auto& actor = Place(
                    graph,
                    {BuildingNode, village, building},
                    scenery,
                    Game::VillageBuilding(village, building),
                    building,
                    [this](int b)
                    {
                        DrawVillageBuilding(b);
                    }
                );
                actor.depthOffset = IsometricView::GroundHalfExtents({25, 23}).y * zoom;
            }
            for (int type = 0; type < 2; ++type)
            {
                Place(
                    graph,
                    {WorksiteNode, village, type},
                    scenery,
                    type == 0 ? Game::HuntingGround(village) : Game::MeditationGround(village),
                    type,
                    [this](int kind)
                    {
                        auto p = Project({});
                        float x = (float)p.x, y = (float)p.y;
                        Oval(x, y, 0, 0, 45, 21, Color(.33f, .37f, .29f, .65f), Color(.25f, .29f, .25f, 0));
                        if (kind == 0)
                        {
                            Stroke(x, y, -16, 0, -16, -32, 3, Color(.37f, .31f, .24f));
                            Stroke(x, y, 16, 0, 16, -32, 3, Color(.37f, .31f, .24f));
                            Stroke(x, y, -20, -30, 20, -30, 3, Color(.45f, .37f, .28f));
                            Tri(x,
                                y,
                                -12,
                                -29,
                                12,
                                -29,
                                0,
                                -7,
                                Color(.47f, .40f, .33f),
                                Color(.40f, .33f, .27f),
                                Color(.30f, .27f, .24f));
                        }
                        else
                        {
                            Oval(x, y, 0, -5, 15, 8, Color(.53f, .56f, .51f), Color(.31f, .36f, .34f));
                            Fire(x, y - 8 * zoom, true);
                        }
                        r.Text(
                            x - 24 * zoom,
                            y + 12 * zoom,
                            kind == 0 ? L"\uc0ac\ub0e5\ud130" : L"\uba85\uc0c1\ud130",
                            Color(.72f, .76f, .66f),
                            12
                        );
                    }
                );
            }
        }
    }

    void UpdateSociety(float dt, bool playerPresent)
    {
        auto& simulation = simulationGraph.GetOrCreate<CallbackActor>({SimulationNode}, 0);
        simulation.renderable = false;
        simulation.update = [this, playerPresent](float step)
        {
            Society::Update(world, step, playerPresent);
        };
        simulationGraph.Update(dt);
    }

    Color Shade(Color c, float light, float alpha = 1)
    {
        return Color(c.r * light, c.g * light, c.b * light, c.a * alpha);
    }

    void Tri(float x, float y, float ax, float ay, float bx, float by, float cx, float cy, Color a, Color b, Color c)
    {
        r.Triangle(x + ax * zoom, y + ay * zoom, x + bx * zoom, y + by * zoom, x + cx * zoom, y + cy * zoom, a, b, c);
    }

    void Oval(float x, float y, float ax, float ay, float rx, float ry, Color a, Color b)
    {
        r.Ellipse(x + ax * zoom, y + ay * zoom, rx * zoom, ry * zoom, a, b);
    }

    void Stroke(float x, float y, float ax, float ay, float bx, float by, float size, Color c)
    {
        r.Line(x + ax * zoom, y + ay * zoom, x + bx * zoom, y + by * zoom, size * zoom, c);
    }

    void Shadow(float x, float y, float rx = 22, float ry = 9)
    {
        Oval(x, y, 10, 1, rx, ry, Color(0.025f, 0.028f, 0.025f, 0.42f), Color(0.025f, 0.028f, 0.025f, 0));
    }

    Color AbilityColor(Game::Ability a)
    {
        const Color colors[] = {
            Color(.65f, .29f, .25f),
            Color(.33f, .59f, .64f),
            Color(.59f, .48f, .63f),
            Color(.80f, .70f, .43f),
            Color(.57f, .61f, .61f),
            Color(.39f, .61f, .40f)
        };
        return colors[(int)a];
    }

    void Grass(unsigned seed)
    {
        auto p = Project({});
        unsigned variant = seed % 16;
        r.CachedMesh(
            Renderer::MeshKind::Grass,
            (std::uint64_t(IsometricView::MeshRevision) << 32) | variant,
            (float)p.x,
            (float)p.y,
            zoom,
            .85f,
            [this, variant]
            {
                // Five tapered, bent blades per cached tuft, with shaded roots and lighter tips.
                for (int blade = 0; blade < 5; ++blade)
                {
                    unsigned h = Game::Hash(variant, blade, 137);
                    double bx = (int)(h % 17) - 8;
                    double by = (int)((h / 17) % 13) - 6;
                    float height = 3.5f + (h / 221 % 7) * .65f;
                    float lean = ((int)(h / 1547 % 11) - 5) * .55f;
                    auto base = IsometricView::Project({bx, by, 0});
                    float x = (float)base.x, y = (float)base.y;
                    float midX = x + lean * .35f, midY = y - height * .6f;
                    Color root(.10f, .24f, .075f), middle(.20f, .37f, .11f);
                    Color tip(.32f + (variant % 3) * .015f, .49f, .18f);
                    r.Triangle(x - .7f, y, x + .7f, y, midX + .45f, midY, root, root, middle);
                    r.Triangle(x - .7f, y, midX + .45f, midY, midX - .45f, midY, root, middle, middle);
                    r.Triangle(midX - .45f, midY, midX + .45f, midY, x + lean, y - height, middle, middle, tip);
                }
            }
        );
    }

    void Rock(float x, float y, float scale, unsigned seed, float opacity = 1)
    {
        if (Art::Environment(r, "SM_ENV_Rock_02_Medium", x, y, zoom * scale, 20, opacity))
        {
            return;
        }
        r.CachedMesh(
            Renderer::MeshKind::Rock,
            seed % 9,
            x,
            y,
            zoom * scale,
            opacity,
            [this, seed]
            {
                GameScene mesh(r, world);
                mesh.zoom = 1;
                mesh.BuildRock(0, 0, 1, seed);
            }
        );
    }

    void BuildRock(float x, float y, float scale, unsigned seed, float opacity = 1)
    {
        float z = zoom;
        zoom *= scale;
        Color top(.49f, .50f, .46f, opacity), left(.32f, .35f, .33f, opacity), right(.23f, .26f, .25f, opacity);
        float peak = 12 + (seed % 9);
        Tri(x, y, -14, 0, -11, -12, 0, -peak, left, top, top);
        Tri(x, y, -14, 0, 0, -peak, 6, -3, left, top, left);
        Tri(x, y, 0, -peak, 13, -9, 6, -3, top, right, left);
        Tri(x, y, 6, -3, 13, -9, 15, 2, left, right, right);
        Stroke(x, y, -8, -10, -1, -12, 0.65f, Shade(top, 1.25f));
        Stroke(x, y, 1, -11, 4, -7, 0.7f, Shade(right, .7f));
        zoom = z;
    }

    void Tree(float x, float y, unsigned seed, float alpha)
    {
        const char* ids[] =
            {"SM_ENV_Tree_01_Oak", "SM_ENV_Tree_02_Pine", "SM_ENV_Tree_03_Dead", "SM_ENV_Tree_04_Young"};
        if (Art::Environment(r, ids[seed % 4], x, y, zoom, 26, alpha))
        {
            return;
        }
        Shadow(x, y, 40, 10);
        r.CachedMesh(
            Renderer::MeshKind::Tree,
            seed,
            x,
            y,
            zoom,
            alpha,
            [this, seed]
            {
                GameScene mesh(r, world);
                mesh.zoom = 1;
                mesh.BuildTree(0, 0, seed, 1);
            }
        );
    }

    void BuildTree(float x, float y, unsigned seed, float alpha)
    {
        float h = 64 + (seed % 38);
        Color bark(.25f, .255f, .22f, alpha), light(.38f, .38f, .30f, alpha);
        Tri(x, y, -6, 2, -3, -h, 6, 0, bark, light, Shade(bark, .6f));
        Stroke(x, y, -1, -10, 1, -h + 8, 1.2f, light);
        for (int i = 0; i < 6; ++i)
        {
            float sign = (i % 2) ? 1.f : -1.f, base = -h * (.3f + i * .10f);
            float end = sign * (15 + (Game::Hash(seed, i, 6) % 15));
            Stroke(x, y, 0, base, end, base - 17, 3.4f - i * .35f, bark);
            Stroke(x, y, end, base - 17, end + sign * 8, base - 34, 1.2f, light);
            Stroke(x, y, end * .6f, base - 10, end * .6f - sign * 6, base - 26, .8f, bark);
        }
        for (int i = 0; i < 4; ++i)
        {
            Stroke(x, y, -4 + i * 3, 0, -16 + i * 11, 4 + (i % 2) * 2, 1.7f, bark);
        }
    }

    void Wall(float x, float y, unsigned seed, float alpha = 1)
    {
        if (Art::Environment(r, "SM_ENV_Ruin_02_WallBroken", x, y, zoom, 16, alpha))
        {
            return;
        }
        Shadow(x, y, 34, 11);
        r.CachedMesh(
            Renderer::MeshKind::Wall,
            (std::uint64_t(IsometricView::MeshRevision) << 32) | (seed % 9),
            x,
            y,
            zoom,
            alpha,
            [this, seed]
            {
                GameScene mesh(r, world);
                mesh.zoom = 1;
                mesh.BuildWall(seed, 1);
            }
        );
    }

    void BuildWall(unsigned seed, float alpha)
    {
        float height = 28 + seed % 3 * 5.f;
        Color front(.37f, .39f, .36f, alpha), top(.54f, .54f, .49f, alpha), side(.24f, .27f, .26f, alpha);
        Prism(20, 6, height, front, side, top);
        for (int row = 1; row <= 3; ++row)
        {
            double z = row * 8;
            Edge({-20, -6, z}, {20, -6, z}, .7f, Shade(side, .6f));
            Edge({-20, -6, z}, {-20, 6, z}, .7f, Shade(front, .6f));
            for (int col = 0; col < 3; ++col)
            {
                double x = -14 + col * 13 + row % 2 * 4;
                Edge({x, -6, z - 8}, {x, -6, z}, .6f, Shade(side, .6f));
            }
        }
        Edge({-20, -6, height}, {20, -6, height}, .9f, Shade(top, 1.15f));
        BuildRock(-22 * zoom, 4 * zoom, .6f, seed, alpha);
    }

    void Person(
        Game::Vec p,
        Game::Ability ability,
        bool player,
        int archetype = 6,
        int id = 0,
        bool crossbow = false,
        bool child = false,
        const CharacterMotion* motion = nullptr
    )
    {
        Game::Vec screen = Project(p);
        float scale = zoom * (child ? .65f : 1.f);
        CharacterMesh::Appearance appearance;
        appearance.facing = player ? playerMotion.facing : CharacterFacing::Front;
        appearance.pose = player ? playerMotion.Pose() : CharacterMotion::IdlePose;
        if (motion && !player)
        {
            appearance.facing = motion->facing;
            appearance.pose = motion->Pose();
        }
        appearance.ability = (unsigned)ability;
        appearance.player = player;
        appearance.crossbow = crossbow;
        appearance.armed = !player && archetype == 0;
        float x = (float)screen.x, y = (float)screen.y;
        Art::DrawRequest imported;
        imported.assets = (!cameraOverride && id > 0 && id <= (int)world.npcs.size())
                              ? world.npcs[id - 1].artParts
                              : Art::Assets().Recipe(player ? 0 : (unsigned)id);
        imported.x = x;
        imported.y = y;
        imported.scale = scale;
        imported.yaw = Art::FacingYaw(appearance.facing);
        bool moving = player ? playerMotion.moving : (motion && motion->moving);
        bool sprinting = player ? playerMotion.sprinting : (motion && motion->sprinting);
        imported.clip = moving ? (sprinting ? "Run" : "Walk") : "Idle";
        imported.seconds = world.time;
        if (Art::Draw(r, imported))
        {
            return;
        }
        r.Ellipse(
            x + 12 * scale,
            y + 5 * scale,
            25 * scale,
            8 * scale,
            Color(.02f, .025f, .025f, .23f),
            Color(.02f, .025f, .025f, 0)
        );
        r.Ellipse(
            x,
            y + scale,
            8 * scale,
            3.5f * scale,
            Color(.015f, .019f, .019f, .48f),
            Color(.02f, .025f, .025f, 0)
        );
        r.CachedMesh(
            Renderer::MeshKind::Character,
            CharacterMesh::Key(appearance),
            x,
            y,
            scale,
            1,
            [this, appearance]
            {
                const auto vertices = CharacterMesh::Build(appearance);
                for (size_t i = 0; i < vertices.size(); i += 3)
                {
                    const auto& a = vertices[i];
                    const auto& b = vertices[i + 1];
                    const auto& c = vertices[i + 2];
                    r.Triangle(
                        a.x,
                        a.y,
                        b.x,
                        b.y,
                        c.x,
                        c.y,
                        Color(a.r, a.g, a.b, a.a),
                        Color(b.r, b.g, b.b, b.a),
                        Color(c.r, c.g, c.b, c.a)
                    );
                }
            }
        );
    }

    void Fire(float x, float y, bool burning)
    {
        Shadow(x, y, 26, 9);
        Stroke(x, y, -12, 2, 12, -4, 4, Color(.25f, .20f, .14f));
        Stroke(x, y, -10, -5, 13, 3, 4, Color(.32f, .27f, .17f));
        for (int i = 0; i < 7; ++i)
        {
            float a = i * 6.283f / 7;
            Rock(x + std::cos(a) * 17 * zoom, y + std::sin(a) * 7 * zoom, .3f, i + 11);
        }
        if (!burning)
        {
            return;
        }
        Oval(x, y, 0, 0, 55, 22, Color(.90f, .46f, .15f, .18f), Color(.8f, .4f, .1f, 0));
        for (int i = 0; i < 5; ++i)
        {
            float flicker = (float)std::sin(world.time * 7 + i * 2) * 3;
            float dx = (i - 2) * 3.f;
            Oval(x, y, dx, -9 - flicker, 5, 12 + flicker, Color(5.0f, 2.8f, .65f, .9f), Color(.9f, .24f, .04f, 0));
        }
    }

    void DrawSite(const Game::Site& site)
    {
        auto p = Project(site.p);
        float x = (float)p.x, y = (float)p.y;
        auto state = Game::State(world, site.key);
        Shadow(x, y, 36, 11);
        switch (site.kind)
        {
            case Game::Camp:
                Tri(x,
                    y,
                    18,
                    -5,
                    37,
                    -47,
                    66,
                    -9,
                    Color(.34f, .36f, .32f),
                    Color(.52f, .53f, .45f),
                    Color(.25f, .28f, .26f));
                Tri(x,
                    y,
                    37,
                    -47,
                    66,
                    -9,
                    46,
                    -3,
                    Color(.47f, .47f, .39f),
                    Color(.25f, .27f, .23f),
                    Color(.17f, .19f, .17f));
                Stroke(x, y, 37, -49, 37, -5, 1.5f, Color(.31f, .27f, .21f));
                Stroke(x, y, 37, -47, 15, 4, 1, Color(.58f, .56f, .44f));
                Fire(x, y, state.stage > 0);
                break;
            case Game::Wounded:
                if (state.stage == 1)
                {
                    Person(site.p, site.ability, false, 1);
                }
                else
                {
                    Oval(x, y, 0, -3, 18, 6, Color(.24f, .29f, .28f), Color(.14f, .18f, .17f));
                    Oval(x, y, -12, -8, 5, 6, Color(.62f, .48f, .38f), Color(.31f, .30f, .25f));
                    Stroke(x, y, -4, -5, 8, -3, 4, Color(.39f, .19f, .16f));
                    Stroke(x, y, 12, -3, 25, 0, 4, Color(.19f, .21f, .20f));
                }
                break;
            case Game::Herb:
                for (int i = 0; i < 11; ++i)
                {
                    float dx = (i - 5) * 2.4f, h = 5 + (i * 7 % 13);
                    Color leaf = world.time >= state.readyAt ? Color(.42f, .54f, .34f) : Color(.26f, .33f, .25f);
                    Stroke(x, y, dx, 1, dx - 3, -h, .8f, leaf);
                    if (world.time >= state.readyAt)
                    {
                        Oval(x, y, dx - 3, -h, 2.8f, 1.7f, Color(.65f, .65f, .52f), leaf);
                    }
                }
                break;
            case Game::Cache:
                Oval(x, y, -17, -9, 9, 11, Color(.14f, .16f, .15f), Color(.37f, .34f, .27f));
                Oval(x, y, -17, -9, 6, 8, Color(.20f, .24f, .21f), Color(.22f, .24f, .20f));
                Stroke(x, y, -17, -18, -17, 0, 1.4f, Color(.47f, .43f, .32f));
                Stroke(x, y, -24, -9, -10, -9, 1.4f, Color(.47f, .43f, .32f));
                for (int i = 0; i < 4; ++i)
                {
                    Stroke(x, y, -10, -7 - i * 4, 22, -13 - i * 4, 3, Color(.34f + i * .025f, .30f + i * .02f, .23f));
                }
                Stroke(x, y, 22, -13, 42, -7, 2, Color(.37f, .33f, .25f));
                if (state.stage == 0)
                {
                    Oval(x, y, 3, -26, 10, 6, Color(.54f, .51f, .40f), Color(.30f, .31f, .27f));
                }
                break;
            case Game::Relic:
                Wall(x - 10 * zoom, y, 21);
                Rock(x + 24 * zoom, y + 8 * zoom, .8f, 71);
                Stroke(x, y, -6, -26, 0, -18, 1, Color(.64f, .65f, .57f));
                Stroke(x, y, 0, -18, 6, -27, 1, Color(.64f, .65f, .57f));
                if (state.stage == 0)
                {
                    Oval(x, y, 0, -19, 3, 5, Color(1.4f, 1.7f, 1.2f, .9f), Color(.5f, .6f, .5f, 0));
                }
                break;
            case Game::Shrine:
                Wall(x - 22 * zoom, y, 7);
                Wall(x + 22 * zoom, y, 9);
                Tri(x,
                    y,
                    -46,
                    -35,
                    0,
                    -68,
                    46,
                    -35,
                    Color(.29f, .33f, .32f),
                    Color(.56f, .56f, .50f),
                    Color(.31f, .35f, .33f));
                Tri(x,
                    y,
                    -30,
                    -36,
                    0,
                    -56,
                    30,
                    -36,
                    Color(.20f, .25f, .23f),
                    Color(.25f, .29f, .26f),
                    Color(.20f, .25f, .23f));
                Rock(x, y + 3 * zoom, 1.1f, 19);
                if (state.stage > 0)
                {
                    Fire(x, y - 12 * zoom, true);
                }
                break;
        }
    }

    void Render(int targetKind, int targetIndex, const std::vector<Game::Site>& sites)
    {
        graph.BeginSync();
        auto root = graph.Group({Root}).Id();
        auto scenery = graph.Group({Scenery}, root).Id();
        auto& terrain = graph.GetOrCreate<CallbackActor>({TerrainNode}, root);
        terrain.layer = ActorLayer::Background;
        terrain.draw = [this](const ActorTransform&)
        {
            r.Terrain((float)world.player.p.x, (float)world.player.p.y, zoom);
        };
        SyncCharacters();
        SyncVillages(scenery);
        int radius = (int)((width * .5f + height) / zoom / 64) + 6;
        int tx = (int)std::floor(world.player.p.x / 32), ty = (int)std::floor(world.player.p.y / 32);
        for (int y = ty - radius; y <= ty + radius; ++y)
        {
            for (int x = tx - radius; x <= tx + radius; ++x)
            {
                Game::Vec position(x * 32.0 + 16, y * 32.0 + 16);
                auto screen = Project(position);
                if (screen.x < -90 || screen.x > width + 90 || screen.y < -50 || screen.y > height + 150)
                {
                    continue;
                }
                if (Game::Obstacle(x, y))
                {
                    unsigned seed = Game::Hash(x, y, 37);
                    auto& obstacle = Place(
                        graph,
                        {ObstacleNode, x, y},
                        scenery,
                        position,
                        seed,
                        [this](unsigned seed)
                        {
                            DrawObstacle(seed);
                        }
                    );
                    obstacle.depthOffset =
                        (seed / 101) % 3 == 2 ? IsometricView::GroundHalfExtents({20, 6}).y * zoom : 0;
                }
                else if (Game::Road(position) > 55 && !Game::SettlementClearing(position))
                {
                    unsigned seed = Game::Hash(x, y, 29);
                    if (seed % 100 >= 62)
                    {
                        continue;
                    }
                    Game::Vec tuft(position.x + (int)(seed / 100 % 23) - 11, position.y + (int)(seed / 2300 % 23) - 11);
                    if (Game::Road(tuft) < 45 || Game::SettlementClearing(tuft))
                    {
                        continue;
                    }
                    Place(
                        graph,
                        {GrassNode, x, y},
                        scenery,
                        tuft,
                        seed,
                        [this](unsigned seed)
                        {
                            Grass(seed);
                        },
                        ActorLayer::Ground
                    );
                }
            }
        }
        ActorId target = 0;
        for (size_t i = 0; i < sites.size(); ++i)
        {
            const auto& site = sites[i];
            auto& actor = Place(
                graph,
                {SiteNode, site.key.first, site.key.second},
                scenery,
                site.p,
                site,
                [this](Game::Site data)
                {
                    data.p = {};
                    DrawSite(data);
                }
            );
            if (targetKind == 1 && targetIndex == (int)i)
            {
                target = actor.Id();
            }
        }
        if (targetKind == 2 && targetIndex >= 0 && targetIndex < (int)world.npcs.size())
        {
            const auto& npc = world.npcs[targetIndex];
            if (Game::Distance(npc.p, world.player.p) < 1400)
            {
                if (auto* actor = graph.Find(ActorKey{NpcNode, npc.id}))
                {
                    target = actor->Id();
                }
            }
        }
        if (target)
        {
            auto& marker = Place(
                graph,
                {TargetNode},
                target,
                {},
                0,
                [this](int)
                {
                    auto p = Project({});
                    r.Ellipse(
                        (float)p.x,
                        (float)p.y,
                        26 * zoom,
                        11 * zoom,
                        Color(.83f, .76f, .48f, .03f),
                        Color(.83f, .76f, .48f, .36f)
                    );
                },
                ActorLayer::Ground
            );
            marker.order = 1;
        }
        graph.EndSync();
        RenderGraph(graph);
    }

private:
    int m_InspectedNpc = 0;
    const ActorTransform* m_ActorTransform = nullptr;
    float m_ProjectionZoom = 1;

    void DrawObstacle(unsigned seed)
    {
        auto p = Project({});
        switch ((seed / 101) % 3)
        {
            case 0:
                Tree((float)p.x, (float)p.y, seed, SceneryOpacity(p, 40, 120));
                break;
            case 1:
                Shadow((float)p.x, (float)p.y);
                Rock((float)p.x, (float)p.y, 1.15f, seed, SceneryOpacity(p, 20, 26));
                break;
            case 2: {
                auto bounds = IsometricView::GroundHalfExtents({20, 6});
                Wall((float)p.x, (float)p.y, seed, SceneryOpacity(p, (float)bounds.x, 60, (float)bounds.y));
                break;
            }
        }
    }
};
