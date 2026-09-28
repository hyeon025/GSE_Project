#pragma once
#include "GameWorld.h"
#include "SceneGraph.h"
#include "WorldActors.h"
#include "CharacterMesh.h"

class GameScene
{
public:
    Renderer& r;
    Game::World& world;
    int width = 1440, height = 900;
    float zoom = 1.4f;
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
        HudNode
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
        double dx = p.x - origin.x, dy = p.y - origin.y;
        return Game::Vec(
            width * .5 + (dx - dy) * projectionZoom,
            height * .55 - (dx + dy) * .5 * projectionZoom - elevation * projectionZoom
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
            if (Game::Distance(n.p, world.player.p) >= 1400)
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
                                false,
                                data.id == world.story.childId
                            );
                        }
                    );
                }
            );
            npc.data = n;
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

    void Rock(float x, float y, float scale, unsigned seed, float opacity = 1)
    {
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
        Shadow(x, y, 34, 11);
        r.CachedMesh(
            Renderer::MeshKind::Wall,
            seed % 9,
            x,
            y,
            zoom,
            alpha,
            [this, seed]
            {
                GameScene mesh(r, world);
                mesh.zoom = 1;
                mesh.BuildWall(0, 0, seed, 1);
            }
        );
    }

    void BuildWall(float x, float y, unsigned seed, float alpha)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                if (row == 3 && col == (int)(seed % 3))
                {
                    continue;
                }
                float a = -24 + col * 15 + (row % 2) * 3, b = -row * 9;
                Color front(.37f, .39f, .36f, alpha), top(.54f, .54f, .49f, alpha), side(.24f, .27f, .26f, alpha);
                Tri(x, y, a, b, a, b - 8, a + 14, b - 8, front, top, front);
                Tri(x, y, a, b, a + 14, b - 8, a + 14, b, front, front, side);
                Stroke(x, y, a + 1, b - 7, a + 13, b - 7, .65f, top);
            }
        }
        BuildRock(x - 22 * zoom, y + 4 * zoom, .6f, seed, alpha);
    }

    void Person(
        Game::Vec p,
        Game::Ability ability,
        bool player,
        int archetype = 6,
        int id = 0,
        bool crossbow = false,
        bool child = false
    )
    {
        Game::Vec screen = Project(p);
        float scale = zoom * (child ? .65f : 1.f);
        CharacterMesh::Appearance appearance;
        appearance.facing = player ? playerMotion.facing : CharacterFacing::Front;
        appearance.pose = player ? playerMotion.Pose() : CharacterMotion::IdlePose;
        appearance.ability = (unsigned)ability;
        appearance.player = player;
        appearance.crossbow = crossbow;
        appearance.armed = !player && archetype == 0;
        float x = (float)screen.x, y = (float)screen.y;
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
                    Place(
                        graph,
                        {ObstacleNode, x, y},
                        scenery,
                        position,
                        Game::Hash(x, y, 37),
                        [this](unsigned seed)
                        {
                            DrawObstacle(seed);
                        }
                    );
                }
                else if (Game::Road(position) > 40)
                {
                    Place(
                        graph,
                        {GrassNode, x, y},
                        scenery,
                        position,
                        Game::Hash(x, y, 29),
                        [this](unsigned seed)
                        {
                            auto p = Project({});
                            for (int i = 0; i < 3; ++i)
                            {
                                float dx = (int)((seed >> (i * 4)) % 21) - 10.f;
                                Stroke(
                                    (float)p.x,
                                    (float)p.y,
                                    dx,
                                    0,
                                    dx - 2,
                                    -3 - (seed % 5),
                                    .65f,
                                    Color(.35f, .40f, .29f, .55f)
                                );
                            }
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
    const ActorTransform* m_ActorTransform = nullptr;
    float m_ProjectionZoom = 1;

    void DrawObstacle(unsigned seed)
    {
        auto p = Project({});
        float alpha = (std::abs(p.x - width * .5) < 36 * zoom && p.y > height * .55 && p.y < height * .55 + 90 * zoom)
                          ? .30f
                          : 1.f;
        switch ((seed / 101) % 3)
        {
            case 0:
                Tree((float)p.x, (float)p.y, seed, alpha);
                break;
            case 1:
                Shadow((float)p.x, (float)p.y);
                Rock((float)p.x, (float)p.y, 1.15f, seed, alpha);
                break;
            case 2:
                Wall((float)p.x, (float)p.y, seed, alpha);
                break;
        }
    }
};
