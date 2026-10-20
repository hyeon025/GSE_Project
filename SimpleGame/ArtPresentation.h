#pragma once
#include "ArtAsset.h"
#include "CharacterMotion.h"
class Renderer;

namespace Art
{
    struct DrawRequest
    {
        std::vector<std::string> assets;
        std::string clip;
        double seconds = 0;
        bool loop = true;
        float x = 0, y = 0, scale = 1, opacity = 1;
        float unitsPerMeter = 30;
        float yaw = 0;
    };

    float FacingYaw(CharacterFacing facing);
    bool Reload();
    bool Draw(Renderer& renderer, const DrawRequest& request);
    bool Environment(
        Renderer& renderer,
        const std::string& id,
        float x,
        float y,
        float scale,
        float unitsPerMeter,
        float opacity = 1
    );
}
