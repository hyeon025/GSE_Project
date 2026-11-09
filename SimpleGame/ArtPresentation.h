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
        bool deferAnimation = false;
        bool priorityAnimation = false; // Reserve prompt pose updates for the controlled player.
        bool preciseDepth = false;
        bool pixelDepth = false; // Per-pixel self-occlusion; enabled automatically for styled characters.
        bool normalizedTime = false;       // seconds is a [0,1) locomotion cycle when enabled.
        std::uint64_t animationStream = 0; // Per-actor fallback history; excluded from the shared mesh key.
        CharacterStyle style;
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
        float opacity = 1,
        bool preciseDepth = false
    );
}
