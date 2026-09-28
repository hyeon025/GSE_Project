#pragma once
#include "CharacterMotion.h"
#include "RenderMeshCache.h"

namespace CharacterMesh
{
    struct Appearance
    {
        CharacterFacing facing = CharacterFacing::Front;
        unsigned pose = CharacterMotion::IdlePose;
        unsigned ability = 3;
        bool player = false, crossbow = false, armed = false;
    };

    std::uint64_t Key(const Appearance& appearance);
    std::vector<RenderMeshCache::Vertex> Build(const Appearance& appearance);
}
