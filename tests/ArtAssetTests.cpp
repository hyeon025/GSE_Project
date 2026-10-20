#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
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
    std::cout << "Art asset regression checks passed\n";
}
