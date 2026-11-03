#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include "CharacterStyle.h"

namespace Art
{
    struct Vec3
    {
        float x = 0, y = 0, z = 0;
    };

    struct Triangle
    {
        Vec3 points[3];
        std::array<float, 4> color = {{1, 1, 1, 1}};
        Vec3 emission;
        bool doubleSided = false;
        PaletteRole paletteRole = PaletteRole::None;
    };

    struct Clip
    {
        std::string name;
        double start = 0, duration = 0;
    };

    struct Matrix
    {
        std::array<float, 16> values;
    };

    // Immutable shared source; every Sample owns its transforms, never mutating the shared skeleton.
    class Model
    {
    public:
        static std::shared_ptr<Model> Load(const std::wstring& path);
        ~Model();
        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        const std::vector<Clip>& Clips() const;
        std::uint64_t Fingerprint() const;
        bool HasMesh(const std::string& name) const;
        std::vector<Triangle>
        Sample(const std::string& mesh, const std::string& clip, double seconds, bool loop = true, unsigned expression = 0) const;
        // Assemble selected meshes in caller order, sharing one skeleton evaluation.
        std::vector<Triangle> SampleMeshes(
            const std::vector<std::string>& meshes, const std::string& clip, double seconds, bool loop = true, unsigned expression = 0) const;
        Matrix Socket(const std::string& node, const std::string& clip, double seconds, bool loop = true) const;

    private:
        Model();
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    struct Asset
    {
        std::string id, kind, slot, mesh;
        std::wstring path;
        int index = 0;
    };

    class Library
    {
    public:
        Library();
        ~Library();
        bool Reload(const std::wstring& root = L"");
        const Asset* Find(const std::string& id) const;
        const Asset* Part(const std::string& slot, int index) const;
        std::shared_ptr<Model> Load(const Asset& asset);
        std::vector<std::string> Recipe(unsigned identity) const;
        std::vector<std::string> PlayerRecipe() const;
        const std::wstring& Root() const;
        const std::string& Error() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    Library& Assets();
}
