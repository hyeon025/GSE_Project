#pragma once
#include "GameWorld.h"

class GameScene
{
public:
    Renderer& r;
    Game::World& world;
    int width = 1440, height = 900;
    float zoom = 1.4f;
    double walk = 0;
    bool cameraOverride = false;
    Game::Vec camera;

    GameScene(Renderer& renderer, Game::World& w)
        : r(renderer),
          world(w)
    {
    }

    Game::Vec Project(Game::Vec p) const
    {
        const Game::Vec& origin = cameraOverride ? camera : world.player.p;
        double dx = p.x - origin.x, dy = p.y - origin.y;
        return Game::Vec(width * 0.5 + (dx - dy) * zoom, height * 0.55 - (dx + dy) * 0.5 * zoom);
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

    void Person(Game::Vec p, Game::Ability ability, bool player, int archetype = 6, int id = 0, bool crossbow = false)
    {
        Game::Vec s = Project(p);
        float x = (float)s.x, y = (float)s.y;
        Color cloth = player ? Color(.34f, .39f, .36f) : Shade(AbilityColor(ability), .63f);
        float step = player ? (float)std::sin(walk) * 3 : std::sin((float)world.time * 1.8f + id) * .35f;
        Shadow(x, y, 20, 7);
        Stroke(x, y, -4, -16, -5 + step, -2, 4, Color(.16f, .17f, .16f));
        Stroke(x, y, 4, -16, 5 - step, -1, 4, Color(.12f, .14f, .13f));
        Oval(x, y, -5 + step, -1, 4, 2, Color(.22f, .21f, .18f), Color(.10f, .11f, .10f));
        Oval(x, y, 5 - step, 0, 4, 2, Color(.22f, .21f, .18f), Color(.10f, .11f, .10f));
        Tri(x, y, -6, -39, -12, -10, 0, -13, Shade(cloth, 1.35f), Shade(cloth, .62f), cloth);
        Tri(x, y, -6, -39, 0, -13, 10, -11, Shade(cloth, 1.35f), cloth, Shade(cloth, .65f));
        Tri(x, y, -6, -39, 10, -11, 7, -37, Shade(cloth, 1.35f), Shade(cloth, .65f), cloth);
        Stroke(x, y, -6, -32, -12, -20 + step * .3f, 4, Shade(cloth, .82f));
        Stroke(x, y, 7, -33, 12, -22 - step * .3f, 4, Shade(cloth, 1.2f));
        Color skin(.64f, .51f, .40f);
        Oval(x, y, 12, -21 - step * .3f, 2, 3, skin, Shade(skin, .65f));
        Stroke(x, y, -7, -22, 7, -23, 2, Color(.25f, .21f, .16f));
        Stroke(x, y, 5, -38, -5, -22, 1.6f, Color(.47f, .40f, .28f));
        Oval(x, y, -8, -24, 4, 5, Color(.33f, .27f, .18f), Color(.17f, .17f, .14f));
        Oval(x, y, 0, -44, 7, 9, Shade(cloth, .60f), Shade(cloth, .35f));
        Oval(x, y, 1, -43, 4, 6, skin, Shade(skin, .65f));
        Tri(x, y, -7, -44, 0, -54, 8, -43, Shade(cloth, 1.4f), cloth, Shade(cloth, .6f));
        Stroke(x, y, 1, -44, 3, -44, .75f, Color(.12f, .13f, .12f));
        if (player)
        {
            if (crossbow)
            {
                Stroke(x, y, 7, -27, 22, -22, 3.0f, Color(.48f, .40f, .28f));
                Stroke(x, y, 13, -33, 17, -17, 1.8f, Color(.62f, .65f, .58f));
                Stroke(x, y, 13, -33, 20, -24, .8f, Color(.71f, .69f, .57f));
                Stroke(x, y, 20, -24, 17, -17, .8f, Color(.71f, .69f, .57f));
            }
            else
            {
                Stroke(x, y, -12, -25, -16, 0, 1.6f, Color(.40f, .34f, .25f));
            }
            Oval(x, y, 3, -34, 1.6f, 1.6f, AbilityColor(ability), Shade(AbilityColor(ability), .5f));
        }
        else if (archetype == 0)
        {
            Stroke(x, y, 12, -26, 15, -8, 2, Color(.62f, .64f, .63f));
        }
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
        r.Terrain((float)world.player.p.x, (float)world.player.p.y, zoom);

        struct Draw
        {
            float depth;
            int kind, index, tx, ty;
        };

        std::vector<Draw> list;
        int radius = (int)((width * .5f + height) / zoom / 64) + 6;
        int tx = (int)std::floor(world.player.p.x / 32), ty = (int)std::floor(world.player.p.y / 32);
        for (int y = ty - radius; y <= ty + radius; ++y)
        {
            for (int x = tx - radius; x <= tx + radius; ++x)
            {
                auto s = Project(Game::Vec(x * 32.0 + 16, y * 32.0 + 16));
                if (s.x < -90 || s.x > width + 90 || s.y < -50 || s.y > height + 150)
                {
                    continue;
                }
                if (Game::Obstacle(x, y))
                {
                    list.push_back({(float)s.y, 0, 0, x, y});
                }
                else if (Game::Road(Game::Vec(x * 32.0 + 16, y * 32.0 + 16)) > 40)
                {
                    unsigned h = Game::Hash(x, y, 29);
                    for (int i = 0; i < 3; ++i)
                    {
                        float dx = (int)((h >> (i * 4)) % 21) - 10.f;
                        Stroke(
                            (float)s.x,
                            (float)s.y,
                            dx,
                            0,
                            dx - 2,
                            -3 - (h % 5),
                            .65f,
                            Color(.35f, .40f, .29f, .55f)
                        );
                    }
                }
            }
        }
        for (size_t i = 0; i < sites.size(); ++i)
        {
            list.push_back({(float)Project(sites[i].p).y, 1, (int)i, 0, 0});
        }
        for (size_t i = 0; i < world.npcs.size(); ++i)
        {
            if (Game::Distance(world.npcs[i].p, world.player.p) < 1400)
            {
                list.push_back({(float)Project(world.npcs[i].p).y, 2, (int)i, 0, 0});
            }
        }
        list.push_back({(float)Project(world.player.p).y, 3, 0, 0, 0});
        if (targetKind >= 0)
        {
            auto p = Project(targetKind == 1 ? sites[targetIndex].p : world.npcs[targetIndex].p);
            r.Ellipse(
                (float)p.x,
                (float)p.y,
                26 * zoom,
                11 * zoom,
                Color(.83f, .76f, .48f, .03f),
                Color(.83f, .76f, .48f, .36f)
            );
        }
        std::stable_sort(
            list.begin(),
            list.end(),
            [](const Draw& a, const Draw& b)
            {
                return a.depth < b.depth;
            }
        );
        for (const auto& d : list)
        {
            if (d.depth < -120 || d.depth > height + 160)
            {
                continue;
            }
            if (d.kind == 0)
            {
                auto p = Project(Game::Vec(d.tx * 32.0 + 16, d.ty * 32.0 + 16));
                unsigned h = Game::Hash(d.tx, d.ty, 37);
                float alpha =
                    (std::abs(p.x - width * .5) < 36 * zoom && p.y > height * .55 && p.y < height * .55 + 90 * zoom)
                        ? .30f
                        : 1.f;
                switch ((h / 101) % 3)
                {
                    case 0:
                        Tree((float)p.x, (float)p.y, h, alpha);
                        break;
                    case 1:
                        Shadow((float)p.x, (float)p.y);
                        Rock((float)p.x, (float)p.y, 1.15f, h, alpha);
                        break;
                    case 2:
                        Wall((float)p.x, (float)p.y, h, alpha);
                        break;
                }
            }
            else if (d.kind == 1)
            {
                DrawSite(sites[d.index]);
            }
            else if (d.kind == 2)
            {
                const auto& n = world.npcs[d.index];
                Person(n.p, n.ability, false, n.archetype, n.id);
            }
            else
            {
                Person(world.player.p, world.player.ability, true);
            }
        }
        r.Flush();
    }
};
