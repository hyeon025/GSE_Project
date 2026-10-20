#include "stdafx.h"
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#define CGLTF_IMPLEMENTATION
#ifdef _MSC_VER
// cgltf uses portable C runtime APIs; scope MSVC's deprecation diagnostic to this vendor header.
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
#include "Dependencies/cgltf.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "Dependencies/json.hpp"
#include "ArtAsset.h"

namespace Art
{
    namespace
    {
        using Json = nlohmann::json;
        using Mat = std::array<float, 16>;

        Mat Identity()
        {
            return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
        }

        Mat Multiply(const Mat& a, const Mat& b)
        {
            Mat result = {};
            for (int c = 0; c < 4; ++c)
            {
                for (int r = 0; r < 4; ++r)
                {
                    for (int k = 0; k < 4; ++k)
                    {
                        result[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
                    }
                }
            }
            return result;
        }

        Vec3 Transform(const Mat& m, Vec3 p)
        {
            return {
                m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]
            };
        }

        std::vector<unsigned char> Bytes(const std::wstring& path)
        {
            std::ifstream in(path.c_str(), std::ios::binary | std::ios::ate);
            if (!in)
            {
                throw std::runtime_error("Cannot open asset");
            }
            auto size = in.tellg();
            if (size <= 0 || size > 256 * 1024 * 1024)
            {
                throw std::runtime_error("Invalid asset size (limit 256 MiB)");
            }
            std::vector<unsigned char> bytes((size_t)size);
            in.seekg(0);
            if (!in.read((char*)bytes.data(), bytes.size()))
            {
                throw std::runtime_error("Incomplete asset read");
            }
            return bytes;
        }

        std::wstring Wide(const std::string& value)
        {
            int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), nullptr, 0);
            if (!size && !value.empty())
            {
                throw std::runtime_error("Invalid UTF-8 asset path");
            }
            std::wstring result(size, L' ');
            if (size)
            {
                MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), &result[0], size);
            }
            return result;
        }

        Json ReadJson(const std::wstring& path)
        {
            auto bytes = Bytes(path);
            return Json::parse(bytes.begin(), bytes.end());
        }

        bool Exists(const std::wstring& path)
        {
            return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
        }

        std::wstring Child(const std::wstring& root, const std::string& relative)
        {
            std::wstring path = Wide(relative);
            std::replace(path.begin(), path.end(), L'/', L'\\');
            if (path.empty() || path[0] == L'\\' || path.find(L':') != std::wstring::npos)
            {
                throw std::runtime_error("Expected relative catalog path");
            }
            for (size_t start = 0; start < path.size();)
            {
                size_t end = path.find(L'\\', start);
                if (path.substr(start, end - start) == L"..")
                {
                    throw std::runtime_error("Catalog path escapes asset root");
                }
                if (end == std::wstring::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return root + L"\\" + path;
        }

        std::string Name(const char* value)
        {
            return value ? value : "";
        }

        std::vector<float> Unpack(const cgltf_accessor* accessor)
        {
            if (!accessor || !accessor->count)
            {
                return {};
            }
            size_t count = cgltf_accessor_unpack_floats(accessor, nullptr, 0);
            if (count > 16000000)
            {
                throw std::runtime_error("Accessor exceeds element limit");
            }
            std::vector<float> values(count);
            if (cgltf_accessor_unpack_floats(accessor, values.data(), count) != count)
            {
                throw std::runtime_error("Unsupported accessor");
            }
            for (float value : values)
            {
                if (!std::isfinite(value))
                {
                    throw std::runtime_error("Non-finite asset value");
                }
            }
            return values;
        }

        void Normalize(float* q)
        {
            float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
            if (length < 1e-8f)
            {
                throw std::runtime_error("Invalid quaternion");
            }
            for (int i = 0; i < 4; ++i)
            {
                q[i] /= length;
            }
        }

        void Slerp(const float* a, const float* b, float t, float* out)
        {
            float end[4] = {b[0], b[1], b[2], b[3]}, dot = 0;
            for (int i = 0; i < 4; ++i)
            {
                dot += a[i] * end[i];
            }
            if (dot < 0)
            {
                dot = -dot;
                for (float& v : end)
                {
                    v = -v;
                }
            }
            float left = 1 - t, right = t;
            if (dot < .9995f)
            {
                float angle = std::acos(std::max(-1.f, std::min(1.f, dot))), divisor = std::sin(angle);
                left = std::sin((1 - t) * angle) / divisor;
                right = std::sin(t * angle) / divisor;
            }
            for (int i = 0; i < 4; ++i)
            {
                out[i] = left * a[i] + right * end[i];
            }
            Normalize(out);
        }
    }

    struct Model::Impl
    {
        cgltf_data* data = nullptr;
        std::vector<unsigned char> sourceBytes;
        std::uint64_t fingerprint = 14695981039346656037ull;
        std::vector<Clip> clips;

        struct Track
        {
            size_t node;
            cgltf_animation_path_type path;
            cgltf_interpolation_type interpolation;
            std::vector<float> times, values;
            size_t width;
        };

        std::vector<std::vector<Track>> tracks;

        struct Primitive
        {
            size_t node;
            std::vector<float> positions, weights, joints;
            std::vector<size_t> indices;
            std::array<float, 4> color = {{1, 1, 1, 1}};
            Vec3 emission;
            bool doubleSided = false;
        };

        std::vector<Primitive> primitives;

        ~Impl()
        {
            cgltf_free(data);
        }

        std::vector<Mat> Pose(const std::string& name, double seconds, bool loop) const
        {
            if (!std::isfinite(seconds))
            {
                throw std::runtime_error("Non-finite animation time");
            }
            std::vector<cgltf_node> nodes(data->nodes, data->nodes + data->nodes_count);
            int clip = -1;
            for (size_t i = 0; i < clips.size(); ++i)
            {
                if (clips[i].name == name)
                {
                    clip = (int)i;
                }
            }
            if (!name.empty() && clip < 0)
            {
                throw std::runtime_error("Animation clip not found: " + name);
            }
            if (clip >= 0)
            {
                const auto& c = clips[clip];
                double offset = std::max(0.0, seconds);
                float time = (float)(c.start + (loop && c.duration > 0 ? std::fmod(offset, c.duration)
                                                                       : std::min(offset, c.duration)));
                for (const auto& track : tracks[clip])
                {
                    auto end = std::upper_bound(track.times.begin(), track.times.end(), time);
                    size_t a = end == track.times.begin() ? 0 : (size_t)(end - track.times.begin() - 1);
                    size_t b = std::min(a + 1, track.times.size() - 1);
                    float dt = track.times[b] - track.times[a];
                    float t = dt > 0 ? std::max(0.f, std::min(1.f, (time - track.times[a]) / dt)) : 0;
                    auto& node = nodes[track.node];
                    float* out = track.path == cgltf_animation_path_type_translation ? node.translation
                                 : track.path == cgltf_animation_path_type_scale     ? node.scale
                                                                                     : node.rotation;
                    bool cubic = track.interpolation == cgltf_interpolation_type_cubic_spline;
                    const float* p = &track.values[(a * (cubic ? 3 : 1) + (cubic ? 1 : 0)) * track.width];
                    const float* q = &track.values[(b * (cubic ? 3 : 1) + (cubic ? 1 : 0)) * track.width];
                    if (track.interpolation == cgltf_interpolation_type_step || a == b)
                    {
                        std::copy(p, p + track.width, out);
                    }
                    else if (cubic)
                    {
                        for (size_t k = 0; k < track.width; ++k)
                        {
                            out[k] = (2 * t * t * t - 3 * t * t + 1) * p[k] +
                                     (t * t * t - 2 * t * t + t) * dt * p[track.width + k] +
                                     (-2 * t * t * t + 3 * t * t) * q[k] +
                                     (t * t * t - t * t) * dt * (q - track.width)[k];
                        }
                    }
                    else if (track.path == cgltf_animation_path_type_rotation)
                    {
                        Slerp(p, q, t, out);
                    }
                    else
                    {
                        for (size_t k = 0; k < track.width; ++k)
                        {
                            out[k] = p[k] + (q[k] - p[k]) * t;
                        }
                    }
                    node.has_matrix = 0;
                    if (track.path == cgltf_animation_path_type_rotation)
                    {
                        Normalize(out);
                        node.has_rotation = 1;
                    }
                    if (track.path == cgltf_animation_path_type_translation)
                    {
                        node.has_translation = 1;
                    }
                    if (track.path == cgltf_animation_path_type_scale)
                    {
                        node.has_scale = 1;
                    }
                }
            }
            std::vector<Mat> world(nodes.size());
            std::vector<unsigned char> visited(nodes.size(), 0);
            std::function<void(size_t, size_t)> visit = [&](size_t index, size_t depth)
            {
                if (depth > 256 || visited[index] == 1)
                {
                    throw std::runtime_error("Invalid node hierarchy");
                }
                if (visited[index] == 2)
                {
                    return;
                }
                visited[index] = 1;
                Mat local;
                cgltf_node_transform_local(&nodes[index], local.data());
                if (nodes[index].parent)
                {
                    size_t parent = (size_t)(nodes[index].parent - data->nodes);
                    visit(parent, depth + 1);
                    world[index] = Multiply(world[parent], local);
                }
                else
                {
                    world[index] = local;
                }
                visited[index] = 2;
            };
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                visit(i, 0);
            }
            return world;
        }
    };

    Model::Model()
        : impl(new Impl)
    {
    }

    Model::~Model() = default;

    const std::vector<Clip>& Model::Clips() const
    {
        return impl->clips;
    }

    std::uint64_t Model::Fingerprint() const
    {
        return impl->fingerprint;
    }

    bool Model::HasMesh(const std::string& name) const
    {
        for (const auto& p : impl->primitives)
        {
            auto& n = impl->data->nodes[p.node];
            if (Name(n.name) == name || Name(n.mesh->name) == name)
            {
                return true;
            }
        }
        return false;
    }

    std::shared_ptr<Model> Model::Load(const std::wstring& path)
    {
        auto model = std::shared_ptr<Model>(new Model);
        auto& m = *model->impl;
        m.sourceBytes = Bytes(path);
        const auto& bytes = m.sourceBytes;
        for (auto byte : bytes)
        {
            m.fingerprint = (m.fingerprint ^ byte) * 1099511628211ull;
        }
        cgltf_options options = {};
        if (cgltf_parse(&options, bytes.data(), bytes.size(), &m.data) != cgltf_result_success)
        {
            throw std::runtime_error("glTF parse failed");
        }
        // The production delivery format is embedded GLB. External buffers are rejected explicitly.
        for (size_t i = 0; i < m.data->buffers_count; ++i)
        {
            if (m.data->buffers[i].uri)
            {
                throw std::runtime_error("Export as embedded GLB (external/data URI buffers unsupported)");
            }
        }
        if (cgltf_load_buffers(&options, m.data, nullptr) != cgltf_result_success ||
            cgltf_validate(m.data) != cgltf_result_success)
        {
            throw std::runtime_error("Invalid GLB buffers/accessors");
        }
        if (m.data->nodes_count > 8192)
        {
            throw std::runtime_error("Node limit exceeded");
        }
        if (m.data->extensions_required_count)
        {
            throw std::runtime_error("Required glTF extensions are unsupported");
        }
        size_t vertexCount = 0;
        for (size_t i = 0; i < m.data->nodes_count; ++i)
        {
            auto& node = m.data->nodes[i];
            if (!node.mesh)
            {
                continue;
            }
            for (size_t j = 0; j < node.mesh->primitives_count; ++j)
            {
                auto& source = node.mesh->primitives[j];
                if (source.type != cgltf_primitive_type_triangles || source.targets_count)
                {
                    throw std::runtime_error("Only triangle meshes without morph targets are supported");
                }
                Impl::Primitive p;
                p.node = i;
                auto positionAccessor = cgltf_find_accessor(&source, cgltf_attribute_type_position, 0);
                if (!positionAccessor || positionAccessor->type != cgltf_type_vec3 ||
                    (source.indices && source.indices->is_sparse))
                {
                    throw std::runtime_error("Invalid POSITION or sparse indices");
                }
                p.positions = Unpack(cgltf_find_accessor(&source, cgltf_attribute_type_position, 0));
                if (p.positions.empty() || p.positions.size() % 3)
                {
                    throw std::runtime_error("Missing POSITION");
                }
                size_t count = p.positions.size() / 3;
                vertexCount += count;
                if (vertexCount > 2000000)
                {
                    throw std::runtime_error("Model vertex limit exceeded");
                }
                p.weights = Unpack(cgltf_find_accessor(&source, cgltf_attribute_type_weights, 0));
                p.joints = Unpack(cgltf_find_accessor(&source, cgltf_attribute_type_joints, 0));
                if (cgltf_find_accessor(&source, cgltf_attribute_type_weights, 1))
                {
                    throw std::runtime_error("Export at most four skin weights per vertex");
                }
                if (node.skin && (p.weights.size() != count * 4 || p.joints.size() != count * 4))
                {
                    throw std::runtime_error("Skin requires JOINTS_0 and WEIGHTS_0");
                }
                size_t indexCount = source.indices ? source.indices->count : count;
                if (indexCount % 3 || indexCount > 6000000)
                {
                    throw std::runtime_error("Invalid triangle index count");
                }
                for (size_t k = 0; k < indexCount; ++k)
                {
                    size_t index = source.indices ? cgltf_accessor_read_index(source.indices, k) : k;
                    if (index >= count)
                    {
                        throw std::runtime_error("Index outside POSITION");
                    }
                    p.indices.push_back(index);
                }
                if (source.material)
                {
                    auto& mat = *source.material;
                    if (mat.pbr_metallic_roughness.base_color_texture.texture || mat.normal_texture.texture ||
                        mat.emissive_texture.texture)
                    {
                        throw std::runtime_error("Texture materials are not supported by the palette renderer");
                    }
                    std::copy(
                        mat.pbr_metallic_roughness.base_color_factor,
                        mat.pbr_metallic_roughness.base_color_factor + 4,
                        p.color.begin()
                    );
                    p.emission = {mat.emissive_factor[0], mat.emissive_factor[1], mat.emissive_factor[2]};
                    float strength = mat.has_emissive_strength ? mat.emissive_strength.emissive_strength : 1;
                    p.emission = {p.emission.x * strength, p.emission.y * strength, p.emission.z * strength};
                    p.doubleSided = mat.double_sided != 0;
                }
                m.primitives.push_back(std::move(p));
            }
        }
        if (m.primitives.empty())
        {
            throw std::runtime_error("GLB contains no triangle meshes");
        }
        for (size_t i = 0; i < m.data->animations_count; ++i)
        {
            auto& animation = m.data->animations[i];
            Clip clip;
            clip.name = Name(animation.name);
            if (clip.name.empty())
            {
                clip.name = "Animation_" + std::to_string(i);
            }
            double first = 1e30, last = 0;
            std::vector<Impl::Track> tracks;
            for (size_t j = 0; j < animation.channels_count; ++j)
            {
                auto& channel = animation.channels[j];
                if (!channel.target_node || channel.target_path == cgltf_animation_path_type_weights)
                {
                    throw std::runtime_error("Unsupported animation target");
                }
                Impl::Track t;
                t.node = (size_t)(channel.target_node - m.data->nodes);
                t.path = channel.target_path;
                t.interpolation = channel.sampler->interpolation;
                t.width = t.path == cgltf_animation_path_type_rotation ? 4 : 3;
                t.times = Unpack(channel.sampler->input);
                t.values = Unpack(channel.sampler->output);
                if (t.times.empty() ||
                    t.values.size() !=
                        t.times.size() * t.width * (t.interpolation == cgltf_interpolation_type_cubic_spline ? 3 : 1))
                {
                    throw std::runtime_error("Invalid animation samples");
                }
                for (size_t k = 1; k < t.times.size(); ++k)
                {
                    if (t.times[k] <= t.times[k - 1])
                    {
                        throw std::runtime_error("Animation times must increase");
                    }
                }
                first = std::min(first, (double)t.times.front());
                last = std::max(last, (double)t.times.back());
                tracks.push_back(std::move(t));
            }
            if (tracks.empty())
            {
                continue;
            }
            clip.start = first;
            clip.duration = last - first;
            m.clips.push_back(clip);
            m.tracks.push_back(std::move(tracks));
        }
        std::cout << "[Art] loaded nodes=" << m.data->nodes_count << " primitives=" << m.primitives.size()
                  << " skins=" << m.data->skins_count << " clips=" << m.clips.size() << '\n';
        return model;
    }

    std::vector<Triangle>
    Model::Sample(const std::string& mesh, const std::string& clip, double seconds, bool loop) const
    {
        auto& m = *impl;
        auto world = m.Pose(clip, seconds, loop);
        std::vector<Triangle> result;
        for (const auto& p : m.primitives)
        {
            auto& node = m.data->nodes[p.node];
            if (!mesh.empty() && Name(node.name) != mesh && Name(node.mesh->name) != mesh)
            {
                continue;
            }
            std::vector<Mat> palette;
            if (node.skin)
            {
                auto inverse = Unpack(node.skin->inverse_bind_matrices);
                if (!inverse.empty() && inverse.size() != node.skin->joints_count * 16)
                {
                    throw std::runtime_error("Invalid inverse bind matrix count");
                }
                for (size_t j = 0; j < node.skin->joints_count; ++j)
                {
                    Mat bind = Identity();
                    if (!inverse.empty())
                    {
                        // The last matrix ends at size(); do not subscript that one-past-end position.
                        std::copy_n(inverse.data() + j * 16, bind.size(), bind.begin());
                    }
                    palette.push_back(Multiply(world[node.skin->joints[j] - m.data->nodes], bind));
                }
            }
            std::vector<Vec3> vertices(p.positions.size() / 3);
            for (size_t i = 0; i < vertices.size(); ++i)
            {
                Vec3 position = {p.positions[i * 3], p.positions[i * 3 + 1], p.positions[i * 3 + 2]};
                if (!node.skin)
                {
                    vertices[i] = Transform(world[p.node], position);
                }
                else
                {
                    Vec3 sum = {};
                    float total = 0;
                    for (size_t k = 0; k < 4; ++k)
                    {
                        float weight = p.weights[i * 4 + k], joint = p.joints[i * 4 + k];
                        if (weight < 0 || joint < 0 || joint >= palette.size() || joint != std::floor(joint))
                        {
                            throw std::runtime_error("Invalid skin weights/joint index");
                        }
                        auto point = Transform(palette[(size_t)joint], position);
                        sum.x += point.x * weight;
                        sum.y += point.y * weight;
                        sum.z += point.z * weight;
                        total += weight;
                    }
                    if (total < 1e-6f)
                    {
                        throw std::runtime_error("Unweighted skinned vertex");
                    }
                    vertices[i] = {sum.x / total, sum.y / total, sum.z / total};
                }
            }
            for (size_t i = 0; i < p.indices.size(); i += 3)
            {
                Triangle triangle;
                for (int k = 0; k < 3; ++k)
                {
                    triangle.points[k] = vertices[p.indices[i + k]];
                }
                triangle.color = p.color;
                triangle.emission = p.emission;
                triangle.doubleSided = p.doubleSided;
                result.push_back(triangle);
            }
        }
        if (result.empty())
        {
            throw std::runtime_error("Selected mesh was not found: " + mesh);
        }
        return result;
    }

    Matrix Model::Socket(const std::string& name, const std::string& clip, double seconds, bool loop) const
    {
        auto world = impl->Pose(clip, seconds, loop);
        for (size_t i = 0; i < impl->data->nodes_count; ++i)
        {
            if (Name(impl->data->nodes[i].name) == name)
            {
                return {world[i]};
            }
        }
        throw std::runtime_error("Socket/bone not found: " + name);
    }

    struct Library::Impl
    {
        std::wstring root;
        std::string error;
        std::map<std::string, Asset> assets;
        std::map<std::pair<std::string, int>, std::string> parts;
        std::map<std::wstring, std::shared_ptr<Model>> models;
        std::vector<std::vector<std::string>> recipes;
    };

    Library::Library()
        : impl(new Impl)
    {
        Reload();
    }

    Library::~Library() = default;

    bool Library::Reload(const std::wstring& requested)
    {
        try
        {
            std::unique_ptr<Impl> next(new Impl);
            next->root = requested;
            if (next->root.empty())
            {
                wchar_t env[32768] = {}, exe[32768] = {}, cwd[32768] = {};
                GetEnvironmentVariableW(L"GSE_ART_ROOT", env, 32768);
                if (env[0])
                {
                    next->root = env;
                }
                GetModuleFileNameW(nullptr, exe, 32768);
                GetCurrentDirectoryW(32768, cwd);
                for (std::wstring base :
                     {std::wstring(cwd), std::wstring(exe).substr(0, std::wstring(exe).find_last_of(L"\\/"))})
                {
                    for (int depth = 0; depth < 8 && next->root.empty(); ++depth)
                    {
                        auto candidate = base + L"\\Art\\LowPoly_AgeOfEchoes";
                        if (Exists(candidate + L"\\Catalog\\mesh_catalog.json"))
                        {
                            next->root = candidate;
                            break;
                        }
                        size_t slash = base.find_last_of(L"\\/");
                        if (slash == std::wstring::npos)
                        {
                            break;
                        }
                        base.resize(slash);
                    }
                }
            }
            if (next->root.empty())
            {
                throw std::runtime_error("Art root not found; set GSE_ART_ROOT");
            }
            auto catalog = ReadJson(next->root + L"\\Catalog\\mesh_catalog.json");
            if (catalog.at("catalog_version").get<int>() != 1 || catalog.at("units") != "meters")
            {
                throw std::runtime_error("Unsupported art catalog contract");
            }
            auto contract = ReadJson(next->root + L"\\Catalog\\assembly_contract.json");
            if (contract.at("version").get<int>() != 1 || catalog.at("glb_axes").at("up") != "+Y" ||
                catalog.at("glb_axes").at("front") != "+Z")
            {
                throw std::runtime_error("Unsupported assembly axes/version");
            }
            for (const auto& entry : catalog.at("assets"))
            {
                Asset a;
                a.id = entry.at("asset_id");
                if (a.id.empty() || a.id.size() > 128 ||
                    a.id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") !=
                        std::string::npos)
                {
                    throw std::runtime_error("Asset ID must be a save-compatible ASCII identifier");
                }
                a.kind = entry.at("kind");
                a.slot = entry.at("slot");
                a.index = entry.at("index");
                a.path = Child(next->root, entry.at("glb_file"));
                if (!next->assets.emplace(a.id, a).second)
                {
                    throw std::runtime_error("Duplicate asset ID");
                }
                if (a.kind == "npc" && !next->parts.emplace(std::make_pair(a.slot, a.index), a.id).second)
                {
                    throw std::runtime_error("Duplicate slot index");
                }
            }
            auto recipes = ReadJson(next->root + L"\\Catalog\\npc_appearance_recipes.json");
            for (const auto& recipe : recipes.at("recipes"))
            {
                std::vector<std::string> ids;
                const auto& parts = recipe.at("parts");
                for (const auto& required : contract.at("required_slots"))
                {
                    if (parts.at(required.get<std::string>()).get<int>() <= 0)
                    {
                        throw std::runtime_error("Missing required appearance slot");
                    }
                }
                if (parts.value("Headgear", 0) > 0 && parts.value("Hair", 0) > 0)
                {
                    throw std::runtime_error("Headgear/Hair contract violation");
                }
                if ((parts.at("Torso") == 2 && parts.at("Arms") != 2) ||
                    (parts.at("Torso") == 4 && parts.at("Arms") != 4))
                {
                    throw std::runtime_error("Torso/Arms contract violation");
                }
                for (auto it = parts.begin(); it != parts.end(); ++it)
                {
                    int index = it.value();
                    if (!index)
                    {
                        continue;
                    }
                    auto found = next->parts.find({it.key(), index});
                    if (found == next->parts.end())
                    {
                        throw std::runtime_error("Recipe references missing slot index");
                    }
                    ids.push_back(found->second);
                }
                if (ids.size() > 16)
                {
                    throw std::runtime_error("Appearance exceeds saved slot limit");
                }
                next->recipes.push_back(std::move(ids));
            }
            // Optional skeletal exports can replace static parts without changing persistent source IDs.
            auto rigPath = next->root + L"\\Catalog\\rig_catalog.json";
            if (Exists(rigPath))
            {
                auto rig = ReadJson(rigPath);
                for (const auto& part : rig.at("npc_part_mapping"))
                {
                    auto found = next->assets.find(part.at("source_asset_id").get<std::string>());
                    if (found == next->assets.end())
                    {
                        continue;
                    }
                    auto path = next->root + L"\\Exports\\Rigged\\NPC_Modular_Rigged.glb";
                    if (Exists(path))
                    {
                        found->second.path = path;
                        found->second.mesh = part.at("rigged_asset_id");
                    }
                }
            }
            auto creatures = next->root + L"\\Catalog\\creature_catalog.json";
            if (Exists(creatures))
            {
                auto creatureCatalog = ReadJson(creatures);
                for (const auto& entry : creatureCatalog.at("species"))
                {
                    Asset a;
                    a.id = "MOB_" + entry.at("species_key").get<std::string>();
                    a.kind = "creature";
                    a.path = Child(next->root, entry.at("glb_file"));
                    a.index = entry.at("species_index");
                    next->assets.emplace(a.id, a);
                }
            }
            std::cout << "[Art] catalog assets=" << next->assets.size() << " recipes=" << next->recipes.size() << '\n';
            impl.swap(next);
            return true;
        }
        catch (const std::exception& e)
        {
            impl->error = e.what();
            std::cerr << "[Art] catalog: " << e.what() << '\n';
            return false;
        }
    }

    const Asset* Library::Find(const std::string& id) const
    {
        auto i = impl->assets.find(id);
        return i == impl->assets.end() ? nullptr : &i->second;
    }

    const Asset* Library::Part(const std::string& slot, int index) const
    {
        auto i = impl->parts.find({slot, index});
        return i == impl->parts.end() ? nullptr : Find(i->second);
    }

    std::shared_ptr<Model> Library::Load(const Asset& a)
    {
        auto found = impl->models.find(a.path);
        if (found != impl->models.end())
        {
            return found->second;
        }
        auto model = Model::Load(a.path);
        impl->models.emplace(a.path, model);
        return model;
    }

    std::vector<std::string> Library::Recipe(unsigned identity) const
    {
        return impl->recipes.empty() ? std::vector<std::string>{} : impl->recipes[identity % impl->recipes.size()];
    }

    const std::wstring& Library::Root() const
    {
        return impl->root;
    }

    const std::string& Library::Error() const
    {
        return impl->error;
    }

    Library& Assets()
    {
        static Library library;
        return library;
    }
}
