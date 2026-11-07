#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include "../SimpleGame/ArtAsset.h"
#include "../SimpleGame/Dependencies/json.hpp"

void Finite(const std::vector<Art::Triangle>& mesh)
{
    assert(!mesh.empty());
    for (const auto& face : mesh)
    {
        for (const auto& p : face.points)
        {
            assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
        }
    }
}

int main()
{
    Art::Library library;
    assert(library.Error().empty());
    auto parts = library.Recipe(1);
    assert(parts.size() >= 6);
    assert(library.Part("Head", 1) && !library.Part("Head", 0));
    const auto* part = library.Part("Head", 1);
    auto source = library.Load(*part);
    assert(source == library.Load(*part));
    auto bind = source->Sample(part->mesh, "", 0);
    Finite(bind);
    // Shared head morphs change only facial features and remain valid under skinning.
    std::set<std::uint64_t> expressions;
    for(unsigned expression=0;expression<Art::Styles::ExpressionCount;++expression)
    {
        auto face=source->Sample(part->mesh,"Run",.23,true,expression);
        Finite(face);assert(face.size()==bind.size());
        std::uint64_t hash=14695981039346656037ull;
        for(const auto& t:face)for(const auto& p:t.points)
        {
            const auto* bytes=reinterpret_cast<const unsigned char*>(&p);
            for(size_t i=0;i<sizeof(p);++i)hash=(hash^bytes[i])*1099511628211ull;
        }
        assert(expressions.insert(hash).second);
    }
    auto hair=library.Part("Hair",1);
    auto defaultHair=source->Sample(hair->mesh,"Run",.23);
    auto expressiveHair=source->Sample(hair->mesh,"Run",.23,true,3);
    assert(defaultHair.size()==expressiveHair.size());
    for(size_t i=0;i<defaultHair.size();++i)for(int k=0;k<3;++k)
    {
        const auto& a=defaultHair[i].points[k];const auto& b=expressiveHair[i].points[k];
        assert(a.x==b.x && a.y==b.y && a.z==b.z);
    }
    assert(!library.PlayerRecipe().empty() && library.PlayerRecipe()!=library.Recipe(0));
    for (unsigned hair = 1; hair <= 5; ++hair)
    {
        for (unsigned outfit = 1; outfit <= 6; ++outfit)
        {
            auto playerParts = library.PlayerRecipe(hair, outfit);
            assert(playerParts.size() == 7);
            assert(playerParts[0] == library.Part("Head", 2)->id);
            assert(playerParts[1] == library.Part("Hair", hair)->id);
            assert(playerParts[2] == library.Part("Torso", outfit)->id);
        }
    }
    assert(library.PlayerRecipe(0, 1).empty() && library.PlayerRecipe(1, 7).empty());
    if (!source->Clips().empty())
    {
        for (const auto& clip : source->Clips())
        {
            Finite(source->Sample(part->mesh, clip.name, clip.duration * .37));
            Finite(source->Sample(part->mesh, clip.name, clip.duration + 1, false));
        }
        auto again = source->Sample(part->mesh, "", 0);
        assert(bind.size() == again.size());
        assert(bind.front().points[0].x == again.front().points[0].x);
        assert(bind.front().points[0].y == again.front().points[0].y);
        assert(bind.front().points[0].z == again.front().points[0].z);
        auto loopA = source->Sample(part->mesh, "Walk", .25);
        double duration = 0;
        for (const auto& clip : source->Clips())
        {
            if (clip.name == "Walk")
            {
                duration = clip.duration;
            }
        }
        auto loopB = source->Sample(part->mesh, "Walk", .25 + duration);
        assert(std::abs(loopA[0].points[0].y - loopB[0].points[0].y) < .0001f);
    }
    // Import every currently cataloged static asset, not only one representative triangle.
    std::ifstream catalogFile((library.Root() + L"\\Catalog\\mesh_catalog.json").c_str());
    nlohmann::json catalog;
    catalogFile >> catalog;
    for (const auto& entry : catalog.at("assets"))
    {
        const auto* asset = library.Find(entry.at("asset_id").get<std::string>());
        assert(asset);
        Finite(library.Load(*asset)->Sample(asset->mesh, "", 0));
    }
    for (const char* id : {"MOB_Wolf", "MOB_Boar", "MOB_Bear", "MOB_Stag", "MOB_Goat", "MOB_Hare", "MOB_AshenRevenant"})
    {
        const auto* asset = library.Find(id);
        if (!asset)
        {
            continue;
        }
        auto model = library.Load(*asset);
        assert(!model->Clips().empty());
        for (const auto& clip : model->Clips())
        {
            Finite(model->Sample("", clip.name, clip.duration * .43, false));
        }
    }
    bool rejected = false;
    try
    {
        source->Sample("missing_mesh", "", 0);
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    assert(rejected);
    // A whole modular actor must match separate part sampling, including order/materials.
    std::vector<std::string> meshes;
    for (const auto& id : parts)
    {
        const auto* asset = library.Find(id);
        assert(library.Load(*asset) == source);
        meshes.push_back(asset->mesh);
    }
    for (const std::string clip : {std::string(), std::string("Run"), std::string("Turn_L90")})
    {
        auto combined = source->SampleMeshes(meshes, clip, .37, false);
        size_t index = 0;
        for (const auto& mesh : meshes)
            for (const auto& face : source->Sample(mesh, clip, .37, false))
            {
                const auto& actual = combined.at(index++);
                assert(actual.color == face.color && actual.doubleSided == face.doubleSided);
                for (int k = 0; k < 3; ++k)
                {
                    assert(actual.points[k].x == face.points[k].x);
                    assert(actual.points[k].y == face.points[k].y);
                    assert(actual.points[k].z == face.points[k].z);
                }
            }
        assert(index == combined.size());
    }
    rejected = false;
    try { source->SampleMeshes({part->mesh, "missing_mesh"}, "Run", .1); }
    catch (const std::exception&) { rejected = true; }
    assert(rejected);
    std::cout << "Art asset regression checks passed\n";
}
