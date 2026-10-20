#define NOMINMAX
#include <Windows.h>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
#include "../SimpleGame/ArtAsset.h"
#include "../SimpleGame/Dependencies/json.hpp"

int main()
{
    // One joint makes its inverse bind matrix both the first and the last entry.
    // Sampling this fixture under the MSVC Debug STL must not subscript vector::size().
    std::vector<unsigned char> binary(180, 0);
    const float positions[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const float weights[] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    const std::uint16_t indices[] = {0, 1, 2};
    const float inverseBind[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::memcpy(binary.data(), positions, sizeof(positions));
    std::memcpy(binary.data() + 60, weights, sizeof(weights));
    std::memcpy(binary.data() + 108, indices, sizeof(indices));
    std::memcpy(binary.data() + 116, inverseBind, sizeof(inverseBind));
    auto document = nlohmann::json::parse(R"json({
        "asset":{"version":"2.0"},
        "buffers":[{"byteLength":180}],
        "bufferViews":[
            {"buffer":0,"byteOffset":0,"byteLength":36},
            {"buffer":0,"byteOffset":36,"byteLength":24},
            {"buffer":0,"byteOffset":60,"byteLength":48},
            {"buffer":0,"byteOffset":108,"byteLength":6},
            {"buffer":0,"byteOffset":116,"byteLength":64}
        ],
        "accessors":[
            {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
            {"bufferView":1,"componentType":5123,"count":3,"type":"VEC4"},
            {"bufferView":2,"componentType":5126,"count":3,"type":"VEC4"},
            {"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"},
            {"bufferView":4,"componentType":5126,"count":1,"type":"MAT4"}
        ],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1,"WEIGHTS_0":2},"indices":3}]}],
        "skins":[{"joints":[1],"inverseBindMatrices":4}],
        "nodes":[{"mesh":0,"skin":0},{"name":"joint","translation":[2,0,0]}],
        "scenes":[{"nodes":[0,1]}],"scene":0
    })json");
    std::string json = document.dump();
    while (json.size() % 4)
    {
        json += ' ';
    }
    const std::uint32_t header[] =
        {0x46546c67, 2, (std::uint32_t)(28 + json.size() + binary.size()), (std::uint32_t)json.size(), 0x4e4f534a};
    const std::uint32_t binHeader[] = {(std::uint32_t)binary.size(), 0x004e4942};
    wchar_t directory[MAX_PATH] = {}, path[MAX_PATH] = {};
    if (!GetTempPathW(MAX_PATH, directory) || !GetTempFileNameW(directory, L"gse", 0, path))
    {
        throw std::runtime_error("Cannot create skin regression fixture");
    }

    struct Cleanup
    {
        const wchar_t* path;

        ~Cleanup()
        {
            DeleteFileW(path);
        }
    } cleanup{path};

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write((const char*)header, sizeof(header));
        out.write(json.data(), json.size());
        out.write((const char*)binHeader, sizeof(binHeader));
        out.write((const char*)binary.data(), binary.size());
        assert(out.good());
    }
    auto model = Art::Model::Load(path);
    auto triangles = model->Sample("", "", 0);
    assert(triangles.size() == 1);
    assert(triangles[0].points[0].x == 2);
    assert(triangles[0].points[1].x == 3);
    assert(triangles[0].points[2].y == 1);
}
