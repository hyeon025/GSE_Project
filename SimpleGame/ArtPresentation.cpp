#include "stdafx.h"
#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <list>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include "ArtPresentation.h"
#include "Renderer.h"
#include "PoseWorker.h"
#include "ProjectedDepthOrder.h"

namespace Art
{
    namespace
    {
        struct Face
        {
            RenderMeshCache::Vertex vertices[3];
            double depth = 0;
            double vertexDepth[3] = {};
        };

        struct CachedPose
        {
            std::shared_ptr<std::vector<RenderMeshCache::Vertex>> vertices;
            std::list<std::uint64_t>::iterator recent;
        };
        std::map<std::uint64_t, CachedPose> poses;
        std::list<std::uint64_t> order;
        size_t poseBytes = 0;
        std::uint64_t poseBuilds = 0, poseEvictions = 0;
        constexpr size_t PoseBudget = 192 * 1024 * 1024;
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
        using Vertices = std::shared_ptr<std::vector<RenderMeshCache::Vertex>>;
        using Sources = std::vector<std::pair<std::string, std::shared_ptr<Model>>>;
        Vertices ProjectPose(const Sources& sources, const DrawRequest& r, double sampleTime)
        {
            std::vector<Face> faces;
            const auto view = Normalize(
                {-(float)IsometricView::AxisY,
                 -(float)IsometricView::AxisX,
                 2 * IsometricView::GroundDepth / IsometricView::HeightScale}
            );
            const auto light = Normalize({-.45f, -.55f, .85f});
            for (size_t begin = 0; begin < sources.size();)
            {
                // Adjacent modular parts in the same GLB share a single pose/palette.
                // Keep caller part order for identical painter ordering at equal depth.
                const auto& source = sources[begin];
                size_t end = begin + 1;
                while (end < sources.size() && sources[end].second == source.second) ++end;
                std::vector<std::string> meshes;
                for (size_t i = begin; i < end; ++i) meshes.push_back(sources[i].first);
                auto triangles = source.second->SampleMeshes(
                    meshes, source.second->Clips().empty() ? "" : r.clip,
                    sampleTime,
                    r.loop,
                    r.style.enabled ? r.style.expression : 0
                );
                begin = end;
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
                    const bool palette = r.style.enabled && triangle.paletteRole!=PaletteRole::None;
                    const float red = palette ? -(float)triangle.paletteRole : Display(triangle.color[0] * brightness + triangle.emission.x);
                    const float green = palette ? brightness : Display(triangle.color[1] * brightness + triangle.emission.y);
                    const float blue = palette ? 0 : Display(triangle.color[2] * brightness + triangle.emission.z);
                    Face face;
                    for (int i = 0; i < 3; ++i)
                    {
                        auto projected = IsometricView::Project({p[i].x, p[i].y, p[i].z});
                        face.vertices[i] = {
                            (float)projected.x,
                            (float)projected.y,
                            red, green, blue,
                            triangle.color[3],
                            0,
                            0
                        };
                        face.vertexDepth[i] = Dot(p[i], view);
                        face.depth += face.vertexDepth[i];
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
            if (r.preciseDepth)
            {
                std::vector<ProjectedDepthOrder::Triangle> triangles;
                for (const auto& face : faces)
                    triangles.push_back({{{face.vertices[0].x,face.vertices[0].y,face.vertexDepth[0]},
                                          {face.vertices[1].x,face.vertices[1].y,face.vertexDepth[1]},
                                          {face.vertices[2].x,face.vertices[2].y,face.vertexDepth[2]}}});
                auto indices = ProjectedDepthOrder::Order(triangles);
                std::vector<Face> sorted; sorted.reserve(faces.size());
                for (auto index : indices) sorted.push_back(faces[index]);
                faces.swap(sorted);
            }
            auto vertices = std::make_shared<std::vector<RenderMeshCache::Vertex>>();
            vertices->reserve(faces.size() * 3);
            for (const auto& face : faces)
            {
                vertices->insert(vertices->end(), face.vertices, face.vertices + 3);
            }
            if (!RenderMeshCache::Valid(*vertices))
            {
                throw std::runtime_error("Projected asset exceeds mesh limits or is empty");
            }
            return vertices;
        }
        struct Prepared { Vertices vertices; std::string error; std::uint64_t serial = 0; };
        std::unique_ptr<PoseWorker<Prepared>> worker;
        struct Previous {
            std::uint64_t key;
            std::weak_ptr<std::vector<RenderMeshCache::Vertex>> vertices;
            std::uint64_t serial;
        };
        std::map<std::uint64_t, Previous> previous;
        std::set<std::uint64_t> rejected;
        std::uint64_t requestSerial = 0;
        void Store(std::uint64_t key, Vertices vertices)
        {
            if (poses.count(key)) return;
            size_t bytes = vertices->size() * sizeof(RenderMeshCache::Vertex);
            while (!order.empty() && (poseBytes + bytes > PoseBudget || poses.size() >= 1024))
            {
                auto old = poses.find(order.front()); order.pop_front();
                poseBytes -= old->second.vertices->size() * sizeof(RenderMeshCache::Vertex);
                poses.erase(old); ++poseEvictions;
            }
            order.push_back(key); poseBytes += bytes;
            poses.emplace(key, CachedPose{std::move(vertices), std::prev(order.end())});
            ++poseBuilds;
        }
        std::array<Color,4> PaletteColors(const CharacterStyle& style)
        {
            auto p=Styles::Palette(style);
            return {{Color(p[0][0],p[0][1],p[0][2]),Color(p[1][0],p[1][1],p[1][2]),
                     Color(p[2][0],p[2][1],p[2][2]),Color(p[3][0],p[3][1],p[3][2])}};
        }
        void DrawPose(Renderer& renderer, std::uint64_t key, const DrawRequest& r, Vertices vertices, bool persistent)
        {
            auto colors=PaletteColors(r.style);
            renderer.CachedMesh(Renderer::MeshKind::Imported, key, r.x, r.y, r.scale, r.opacity,
                [&renderer, vertices] {
                    for (size_t i=0; i<vertices->size(); i+=3) {
                        const auto& a=(*vertices)[i]; const auto& b=(*vertices)[i+1]; const auto& c=(*vertices)[i+2];
                        renderer.Triangle(a.x,a.y,b.x,b.y,c.x,c.y,{a.r,a.g,a.b,a.a},{b.r,b.g,b.b,b.a},{c.r,c.g,c.b,c.a});
                    }
                }, persistent,r.style.enabled ? &colors : nullptr);
        }
        void Remember(std::uint64_t family, std::uint64_t key, Vertices vertices, std::uint64_t serial)
        {
            auto last = previous.find(family);
            if (last != previous.end() && last->second.serial > serial) return;
            if (previous.size() >= 2048 && !previous.count(family)) previous.clear();
            previous[family] = Previous{key, vertices, serial};
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
        worker.reset();
        previous.clear();
        rejected.clear();
        requestSerial = 0;
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
            const auto serial = ++requestSerial;
            if (worker) {
                PoseWorker<Prepared>::Result result;
                while (worker->Poll(result)) {
                    if (!result.value.error.empty()) { rejected.insert(result.key); continue; }
                    Store(result.key, result.value.vertices);
                    Remember(result.family, result.key, poses.at(result.key).vertices, result.value.serial);
                }
            }
            Sources sources;
            std::uint64_t key = 14695981039346656037ull;
            Hash(key, &PresentationRevision, sizeof(PresentationRevision));
            const unsigned isoRevision = IsometricView::MeshRevision;
            Hash(key, &isoRevision, sizeof(isoRevision));
            Hash(key, &r.unitsPerMeter, sizeof(float));
            Hash(key, &r.yaw, sizeof(float));
            Hash(key, &r.loop, sizeof(r.loop));
            if (r.preciseDepth) { const unsigned depthRevision=1; HashString(key,"precise_depth");Hash(key,&depthRevision,sizeof(depthRevision)); }
            if(r.style.enabled)
            {
                HashString(key,"character_style");
                const unsigned revision=Styles::Revision,expression=r.style.expression%Styles::ExpressionCount;
                Hash(key,&revision,sizeof(revision));Hash(key,&expression,sizeof(expression));
                // Colors belong to the draw instance, never the pose cache key.
            }
            std::uint64_t family = key;
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
                    HashString(family, clip);
                    Hash(key, &frame, sizeof(frame));
                    animated = true;
                }
                HashString(key, id);
                HashString(key, asset->mesh);
                auto fingerprint = model->Fingerprint();
                Hash(key, &fingerprint, sizeof(fingerprint));
                HashString(family, id); HashString(family, asset->mesh);
                Hash(family, &fingerprint, sizeof(fingerprint));
                sources.push_back({asset->mesh, model});
            }
            auto colors=PaletteColors(r.style);
            auto palette=r.style.enabled ? &colors : nullptr;
            if (renderer.TryCachedMesh(Renderer::MeshKind::Imported, key, r.x, r.y, r.scale, r.opacity,palette))
            {
                // A later cache miss must reuse the pose actually displayed most recently.
                auto cpu = poses.find(key);
                Remember(family, key, cpu == poses.end() ? Vertices{} : cpu->second.vertices, serial);
                return true;
            }
            auto cached = poses.find(key);
            if (cached == poses.end() && animated && r.deferAnimation && !rejected.count(key))
            {
                auto last = previous.find(family);
                if (last != previous.end())
                {
                    auto vertices = last->second.vertices.lock();
                    bool resident = renderer.TryCachedMesh(Renderer::MeshKind::Imported, last->second.key,
                                                           r.x,r.y,r.scale,r.opacity,palette);
                    if (vertices || resident)
                    {
                        if (!worker) worker.reset(new PoseWorker<Prepared>());
                        auto request = r;
                        worker->Submit(family,key,[sources,request,sampleTime,serial] {
                            Prepared result;
                            result.serial = serial;
                            try { result.vertices = ProjectPose(sources,request,sampleTime); }
                            catch (const std::exception& e) { result.error = e.what(); }
                            return result;
                        });
                        if (!resident) DrawPose(renderer,last->second.key,r,vertices,false);
                        return true;
                    }
                }
            }
            if (cached == poses.end())
            {
                auto vertices = ProjectPose(sources,r,sampleTime);
                Store(key,vertices);
                cached = poses.find(key);
            }
            order.splice(order.end(), order, cached->second.recent);
            auto vertices = cached->second.vertices;
            Remember(family,key,vertices,serial);
            DrawPose(renderer,key,r,vertices,!animated);
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
    Environment(Renderer& renderer, const std::string& id, float x, float y, float scale, float units, float opacity, bool preciseDepth)
    {
        DrawRequest r;
        r.assets = {id};
        r.x = x;
        r.y = y;
        r.scale = scale;
        r.unitsPerMeter = units;
        r.opacity = opacity;
        r.preciseDepth = preciseDepth;
        return Draw(renderer, r);
    }
}
