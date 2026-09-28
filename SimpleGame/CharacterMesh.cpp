#include "stdafx.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include "CharacterMesh.h"

namespace CharacterMesh
{
    namespace
    {
        struct Vec
        {
            float x, y, z;

            Vec operator+(Vec b) const
            {
                return {x + b.x, y + b.y, z + b.z};
            }

            Vec operator-(Vec b) const
            {
                return {x - b.x, y - b.y, z - b.z};
            }

            Vec operator*(float s) const
            {
                return {x * s, y * s, z * s};
            }
        };

        float Dot(Vec a, Vec b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        Vec Cross(Vec a, Vec b)
        {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }

        Vec Normalize(Vec value)
        {
            return value * (1.f / std::max(.00001f, std::sqrt(Dot(value, value))));
        }

        struct SurfaceVertex
        {
            Vec p, n;
        };

        struct Triangle
        {
            RenderMeshCache::Vertex vertices[3];
            float depth;
        };

        using Tint = std::array<float, 3>;

        class Builder
        {
        public:
            explicit Builder(CharacterFacing facing)
            {
                float angle = facing == CharacterFacing::Back    ? 3.14159265f
                              : facing == CharacterFacing::Left  ? -1.57079633f
                              : facing == CharacterFacing::Right ? 1.57079633f
                                                                 : 0;
                c = std::cos(angle);
                s = std::sin(angle);
                triangles.reserve(1400);
            }

            void Ellipsoid(
                Vec center,
                Vec radius,
                Tint color,
                float shine = .02f,
                Vec axis = {0, 0, 1},
                int rings = 5,
                int sides = 10
            )
            {
                axis = Normalize(axis);
                Vec reference = std::abs(axis.y) > .95f ? Vec{1, 0, 0} : Vec{0, 1, 0};
                Vec right = Normalize(Cross(reference, axis));
                Vec forward = Normalize(Cross(axis, right));
                auto sample = [&](int row, int col)
                {
                    float latitude = -1.57079633f + row * 3.14159265f / rings;
                    float longitude = col * 6.2831853f / sides;
                    float x = std::cos(latitude) * std::cos(longitude);
                    float y = std::cos(latitude) * std::sin(longitude);
                    float z = std::sin(latitude);
                    return SurfaceVertex{
                        center + right * (x * radius.x) + forward * (y * radius.y) + axis * (z * radius.z),
                        Normalize(right * (x / radius.x) + forward * (y / radius.y) + axis * (z / radius.z))
                    };
                };
                for (int row = 0; row < rings; ++row)
                {
                    for (int col = 0; col < sides; ++col)
                    {
                        auto a = sample(row, col), b = sample(row, col + 1);
                        auto d = sample(row + 1, col), e = sample(row + 1, col + 1);
                        if (row > 0)
                        {
                            Add(a, b, d, color, shine);
                        }
                        if (row + 1 < rings)
                        {
                            Add(b, e, d, color, shine);
                        }
                    }
                }
            }

            void Limb(Vec from, Vec to, float radius, Tint color, float shine = .02f)
            {
                Vec delta = to - from;
                float length = std::sqrt(Dot(delta, delta));
                Ellipsoid((from + to) * .5f, {radius, radius * .85f, length * .5f + radius * .4f}, color, shine, delta);
            }

            std::vector<RenderMeshCache::Vertex> Finish()
            {
                std::stable_sort(
                    triangles.begin(),
                    triangles.end(),
                    [](const Triangle& a, const Triangle& b)
                    {
                        return a.depth > b.depth;
                    }
                );
                std::vector<RenderMeshCache::Vertex> result;
                result.reserve(triangles.size() * 3);
                for (const auto& triangle : triangles)
                {
                    result.insert(result.end(), std::begin(triangle.vertices), std::end(triangle.vertices));
                }
                return result;
            }

        private:
            float c = 1, s = 0;
            std::vector<Triangle> triangles;

            Vec Rotate(Vec p) const
            {
                return {p.x * c - p.y * s, p.x * s + p.y * c, p.z};
            }

            void Add(SurfaceVertex a, SurfaceVertex b, SurfaceVertex d, Tint color, float shine)
            {
                SurfaceVertex vertices[] = {a, b, d};
                Vec view = Normalize({0, -1, .5f});
                Vec normal = Rotate(a.n + b.n + d.n);
                if (Dot(normal, view) <= 0)
                {
                    return;
                }
                Triangle triangle = {};
                Vec light = Normalize({-.6f, -.4f, .9f});
                Vec half = Normalize(light + view);
                for (int i = 0; i < 3; ++i)
                {
                    Vec p = Rotate(vertices[i].p), n = Rotate(vertices[i].n);
                    float diffuse = std::max(0.f, Dot(n, light));
                    float rim = std::pow(1 - std::max(0.f, Dot(n, view)), 3.f) * .07f;
                    float specular = std::pow(std::max(0.f, Dot(n, half)), shine > .15f ? 32.f : 12.f) * shine;
                    float occlusion = .84f + .16f * std::min(1.f, std::max(0.f, p.z) / 28.f);
                    float intensity = (.40f + .67f * diffuse + rim) * occlusion;
                    triangle.vertices[i] = {
                        p.x,
                        -p.y * .5f - p.z,
                        color[0] * intensity + specular,
                        color[1] * intensity + specular,
                        color[2] * intensity + specular,
                        1,
                        0,
                        0
                    };
                    triangle.depth += (p.y - p.z * .5f) / 3;
                }
                triangles.push_back(triangle);
            }
        };
    }

    std::uint64_t Key(const Appearance& a)
    {
        // Appearance revision is independent of the generic on-disk mesh format.
        return (std::uint64_t(1) << 32) | (std::uint64_t(a.facing) << 16) | (std::uint64_t(a.pose) << 8) |
               (std::uint64_t(a.ability) << 3) | (a.player ? 4 : 0) | (a.crossbow ? 2 : 0) | (a.armed ? 1 : 0);
    }

    std::vector<RenderMeshCache::Vertex> Build(const Appearance& appearance)
    {
        Builder model(appearance.facing);
        const Tint abilities[] = {
            {.55f, .25f, .22f},
            {.27f, .49f, .54f},
            {.45f, .37f, .51f},
            {.62f, .54f, .34f},
            {.45f, .49f, .48f},
            {.31f, .49f, .33f}
        };
        Tint cloth = appearance.player ? Tint{.29f, .36f, .34f} : abilities[std::min(5u, appearance.ability)];
        Tint leather = {.23f, .18f, .14f}, boots = {.15f, .14f, .12f}, skin = {.61f, .46f, .35f};
        Tint steel = {.48f, .52f, .53f}, linen = {.48f, .47f, .40f};
        float gait = appearance.pose < 8 ? std::sin(appearance.pose * 6.2831853f / 8) : 0;
        float bob = appearance.pose < 8 ? std::abs(gait) * .65f : 0;
        for (int side : {-1, 1})
        {
            float stride = gait * side * 4.2f;
            Vec hip = {side * 3.4f, 0, 25 + bob};
            Vec knee = {side * 3.6f, stride * .55f, 14.5f + bob};
            Vec ankle = {side * 3.8f, stride, 4 + std::max(0.f, stride) * .35f};
            model.Limb(hip, knee, 3.0f, cloth);
            model.Limb(knee, ankle, 2.2f, boots, .05f);
            model.Ellipsoid({ankle.x, ankle.y - 1.7f, ankle.z - 1.3f}, {2.4f, 4.0f, 2.1f}, boots, .08f);
            Vec shoulder = {side * 7.2f, 0, 41 + bob};
            Vec elbow = {side * 8.5f, -stride * .65f, 32 + bob};
            Vec hand = {side * 8.5f, -1 - stride, 25 + bob};
            if (appearance.crossbow)
            {
                elbow.y = -3;
                hand = {side * 5.f, -6, 30 + bob};
            }
            model.Limb(shoulder, elbow, 2.6f, cloth);
            model.Limb(elbow, hand, 2.f, leather, .08f);
            model.Ellipsoid(hand, {1.7f, 1.6f, 2.1f}, skin, .05f);
        }
        // Rounded torso, layered tunic, hood and equipment are genuine rotated 3D surfaces.
        model.Ellipsoid({0, 0, 34 + bob}, {7, 4.0f, 12}, cloth, .015f);
        model.Ellipsoid({0, .8f, 25 + bob}, {7.7f, 4.7f, 6}, cloth, .01f);
        model.Ellipsoid({0, 0, 29 + bob}, {7.2f, 4.2f, 1.1f}, leather, .05f);
        model.Ellipsoid({0, -4.15f, 29 + bob}, {1.1f, .4f, 1.3f}, steel, .25f);
        model.Ellipsoid({0, 2.3f, 37 + bob}, {7.5f, 3.2f, 10.5f}, cloth, .01f);
        model.Ellipsoid({0, 4.8f, 35 + bob}, {4.1f, 2.0f, 6.8f}, leather, .04f);
        model.Ellipsoid({0, 0, 47.8f + bob}, {4.7f, 4.3f, 6.5f}, cloth, .01f, {0, 0, 1}, 6, 12);
        model.Ellipsoid({0, -3.0f, 47.1f + bob}, {3.1f, 1.6f, 4.1f}, skin, .06f, {0, 0, 1}, 6, 12);
        model.Ellipsoid({0, -4.45f, 47.0f + bob}, {.65f, .9f, 1.15f}, skin, .08f);
        for (int side : {-1, 1})
        {
            model.Ellipsoid({side * 1.3f, -4.25f, 48.2f + bob}, {.55f, .3f, .25f}, boots, 0, {0, 0, 1}, 3, 6);
        }
        model.Ellipsoid({0, -2.1f, 43.0f + bob}, {3.2f, 2.2f, 1.4f}, linen, .01f);
        model.Limb({-5, -3.2f, 41 + bob}, {4, -4.0f, 27 + bob}, .65f, leather);
        model.Ellipsoid({-5, -3.0f, 37 + bob}, {.8f, .6f, 1.1f}, abilities[std::min(5u, appearance.ability)], .24f);
        if (appearance.crossbow)
        {
            model.Limb({0, -8, 25 + bob}, {0, -8, 36 + bob}, 1.0f, leather, .08f);
            model.Limb({-7, -8, 33 + bob}, {7, -8, 33 + bob}, .65f, steel, .25f);
            model.Limb({-7, -8, 33 + bob}, {0, -9, 28 + bob}, .18f, linen);
            model.Limb({7, -8, 33 + bob}, {0, -9, 28 + bob}, .18f, linen);
        }
        else if (appearance.player || appearance.armed)
        {
            model.Limb({9, -1, 3}, {9, -1, 30 + bob}, .65f, appearance.armed ? steel : leather, .1f);
        }
        return model.Finish();
    }
}
