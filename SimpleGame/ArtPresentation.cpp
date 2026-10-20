#include "stdafx.h"
#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include "ArtPresentation.h"
#include "Renderer.h"

namespace Art
{
    namespace
    {
        struct Face
        {
            RenderMeshCache::Vertex vertices[3];
            double depth = 0;
        };

        std::map<std::uint64_t, std::shared_ptr<std::vector<RenderMeshCache::Vertex>>> poses;
        std::deque<std::uint64_t> order;
        size_t poseBytes = 0;
        std::uint64_t poseBuilds = 0, poseEvictions = 0;
        constexpr size_t PoseBudget = 96 * 1024 * 1024;
        constexpr unsigned PresentationRevision = 1;
        std::set<std::string> failures;

        void Hash(std::uint64_t& hash, const void* data, size_t count)
        {
            const auto* bytes = (const unsigned char*)data;
            for (size_t i = 0; i < count; ++i)
            {
                hash = (hash ^ bytes[i]) * 1099511628211ull;
            }
        }

        void HashString(std::uint64_t& hash, const std::string& value)
        {
            Hash(hash, value.data(), value.size());
            const char zero = 0;
            Hash(hash, &zero, 1);
        }

        float Display(float linear)
        {
            return linear <= 1 ? std::pow(std::max(0.f, linear), 1 / 2.2f) : linear;
        }

        Vec3 Cross(Vec3 a, Vec3 b)
        {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }

        float Dot(Vec3 a, Vec3 b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        Vec3 Normalize(Vec3 p)
        {
            float length = std::sqrt(Dot(p, p));
            if (length < 1e-8f)
            {
                return {};
            }
            return {p.x / length, p.y / length, p.z / length};
        }

        Vec3 Convert(Vec3 glb, const DrawRequest& r)
        {
            // glTF +Y up/+Z forward -> game +Z up/-Y forward. No per-part recentering.
            float x = glb.x, y = -glb.z;
            return {
                (x * std::cos(r.yaw) - y * std::sin(r.yaw)) * r.unitsPerMeter,
                (x * std::sin(r.yaw) + y * std::cos(r.yaw)) * r.unitsPerMeter,
                glb.y * r.unitsPerMeter
            };
        }
    }

    float FacingYaw(CharacterFacing facing)
    {
        IsometricView::Point input = {0, 1};
        if (facing == CharacterFacing::Back)
        {
            input = {0, -1};
        }
        if (facing == CharacterFacing::Left)
        {
            input = {-1, 0};
        }
        if (facing == CharacterFacing::Right)
        {
            input = {1, 0};
        }
        auto direction = IsometricView::GroundFromScreen(input);
        return (float)(std::atan2(direction.y, direction.x) + 1.5707963267948966);
    }

    bool Reload()
    {
        if (!Assets().Reload())
        {
            return false;
        }
        poses.clear();
        order.clear();
        poseBytes = 0;
        failures.clear();
        return true;
    }

    bool Draw(Renderer& renderer, const DrawRequest& r)
    {
        if (r.assets.empty())
        {
            return false;
        }
        std::string failure = r.assets.front() + "/" + r.clip;
        if (failures.count(failure))
        {
            return false;
        }
        try
        {
            std::vector<std::pair<const Asset*, std::shared_ptr<Model>>> sources;
            std::uint64_t key = 14695981039346656037ull;
            Hash(key, &PresentationRevision, sizeof(PresentationRevision));
            const unsigned isoRevision = IsometricView::MeshRevision;
            Hash(key, &isoRevision, sizeof(isoRevision));
            Hash(key, &r.unitsPerMeter, sizeof(float));
            Hash(key, &r.yaw, sizeof(float));
            Hash(key, &r.loop, sizeof(r.loop));
            bool animated = false;
            // A finite shared pose atlas: at most 120 samples per clip, not one mesh per NPC/frame.
            double sampleTime = 0;
            for (const auto& id : r.assets)
            {
                const auto* asset = Assets().Find(id);
                if (!asset)
                {
                    return false;
                }
                auto model = Assets().Load(*asset);
                std::string clip = r.clip;
                if (model->Clips().empty())
                {
                    clip.clear();
                }
                if (!clip.empty())
                {
                    auto found = std::find_if(
                        model->Clips().begin(),
                        model->Clips().end(),
                        [&](const Clip& c)
                        {
                            return c.name == clip;
                        }
                    );
                    if (found == model->Clips().end())
                    {
                        throw std::runtime_error("Missing clip: " + clip);
                    }
                    double time = std::max(0.0, r.seconds);
                    time = r.loop && found->duration > 0 ? std::fmod(time, found->duration)
                                                         : std::min(time, found->duration);
                    unsigned samples = (unsigned)std::max(1.0, std::min(120.0, std::ceil(found->duration * 15)));
                    unsigned frame = found->duration > 0 ? (unsigned)std::floor(time / found->duration * samples) : 0;
                    sampleTime = found->duration * frame / samples;
                    HashString(key, clip);
                    Hash(key, &frame, sizeof(frame));
                    animated = true;
                }
                HashString(key, id);
                HashString(key, asset->mesh);
                auto fingerprint = model->Fingerprint();
                Hash(key, &fingerprint, sizeof(fingerprint));
                sources.push_back({asset, model});
            }
            auto cached = poses.find(key);
            if (cached == poses.end())
            {
                std::vector<Face> faces;
                const auto view = Normalize(
                    {-(float)IsometricView::AxisY,
                     -(float)IsometricView::AxisX,
                     2 * IsometricView::GroundDepth / IsometricView::HeightScale}
                );
                const auto light = Normalize({-.45f, -.55f, .85f});
                for (const auto& source : sources)
                {
                    auto triangles = source.second->Sample(
                        source.first->mesh,
                        source.second->Clips().empty() ? "" : r.clip,
                        sampleTime,
                        r.loop
                    );
                    for (const auto& triangle : triangles)
                    {
                        Vec3 p[3];
                        for (int i = 0; i < 3; ++i)
                        {
                            p[i] = Convert(triangle.points[i], r);
                        }
                        Vec3 ab = {p[1].x - p[0].x, p[1].y - p[0].y, p[1].z - p[0].z},
                             ac = {p[2].x - p[0].x, p[2].y - p[0].y, p[2].z - p[0].z};
                        auto normal = Normalize(Cross(ab, ac));
                        float visible = Dot(normal, view);
                        if (!triangle.doubleSided && visible <= 0)
                        {
                            continue;
                        }
                        if (visible < 0)
                        {
                            normal = {-normal.x, -normal.y, -normal.z};
                        }
                        float brightness = .35f + .75f * std::max(0.f, Dot(normal, light));
                        Face face;
                        for (int i = 0; i < 3; ++i)
                        {
                            auto projected = IsometricView::Project({p[i].x, p[i].y, p[i].z});
                            face.vertices[i] = {
                                (float)projected.x,
                                (float)projected.y,
                                Display(triangle.color[0] * brightness + triangle.emission.x),
                                Display(triangle.color[1] * brightness + triangle.emission.y),
                                Display(triangle.color[2] * brightness + triangle.emission.z),
                                triangle.color[3],
                                0,
                                0
                            };
                            face.depth += Dot(p[i], view);
                        }
                        faces.push_back(face);
                    }
                }
                // Painter order within one assembled actor includes all slots and materials together.
                std::stable_sort(
                    faces.begin(),
                    faces.end(),
                    [](const Face& a, const Face& b)
                    {
                        return a.depth < b.depth;
                    }
                );
                auto vertices = std::make_shared<std::vector<RenderMeshCache::Vertex>>();
                for (const auto& face : faces)
                {
                    vertices->insert(vertices->end(), face.vertices, face.vertices + 3);
                }
                if (!RenderMeshCache::Valid(*vertices))
                {
                    throw std::runtime_error("Projected asset exceeds mesh limits or is empty");
                }
                size_t bytes = vertices->size() * sizeof(RenderMeshCache::Vertex);
                while (!order.empty() && (poseBytes + bytes > PoseBudget || poses.size() >= 1024))
                {
                    auto old = poses.find(order.front());
                    order.pop_front();
                    if (old != poses.end())
                    {
                        poseBytes -= old->second->size() * sizeof(RenderMeshCache::Vertex);
                        poses.erase(old);
                        ++poseEvictions;
                    }
                }
                order.push_back(key);
                poseBytes += bytes;
                cached = poses.emplace(key, std::move(vertices)).first;
                if (++poseBuilds % 64 == 0)
                {
                    std::cout << "[ArtPose] builds=" << poseBuilds << " cached=" << poses.size()
                              << " bytes=" << poseBytes << " evictions=" << poseEvictions << '\n';
                }
            }
            auto vertices = cached->second;
            renderer.CachedMesh(
                Renderer::MeshKind::Imported,
                key,
                r.x,
                r.y,
                r.scale,
                r.opacity,
                [&renderer, vertices]
                {
                    for (size_t i = 0; i < vertices->size(); i += 3)
                    {
                        const auto& a = (*vertices)[i];
                        const auto& b = (*vertices)[i + 1];
                        const auto& c = (*vertices)[i + 2];
                        renderer.Triangle(
                            a.x,
                            a.y,
                            b.x,
                            b.y,
                            c.x,
                            c.y,
                            {a.r, a.g, a.b, a.a},
                            {b.r, b.g, b.b, b.a},
                            {c.r, c.g, c.b, c.a}
                        );
                    }
                },
                !animated
            );
            return true;
        }
        catch (const std::exception& e)
        {
            failures.insert(failure);
            std::cerr << "[Art] " << failure << ": " << e.what() << " (procedural fallback)\n";
            return false;
        }
    }

    bool
    Environment(Renderer& renderer, const std::string& id, float x, float y, float scale, float units, float opacity)
    {
        DrawRequest r;
        r.assets = {id};
        r.x = x;
        r.y = y;
        r.scale = scale;
        r.unitsPerMeter = units;
        r.opacity = opacity;
        return Draw(renderer, r);
    }
}
