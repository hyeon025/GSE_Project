#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "IsometricView.h"

enum class CharacterFacing : std::uint32_t
{
    Front,
    Back,
    Left,
    Right
};

struct CharacterMotion
{
    CharacterFacing facing = CharacterFacing::Front;
    double phase = 0;
    double stepPhase = 0, deltaX = 0, deltaY = 0;
    bool moving = false;
    bool sprinting = false;
    static constexpr unsigned IdlePose = 8;
    static constexpr double Cycle = 6.283185307179586;
    // Gameplay strides in world units. Keep the existing actor speeds and map scale.
    static constexpr double WalkStride = 85, RunStride = 92;

    void Update(double inputX, double inputY, double travelX, double travelY, bool run = false)
    {
        deltaX = travelX;
        deltaY = travelY;
        double travel = std::hypot(deltaX, deltaY);
        moving = travel > .001;
        sprinting = moving && run;
        double x = moving ? deltaX : inputX;
        double y = moving ? deltaY : inputY;
        // Compare screen-space axes, compensating for isometric vertical compression.
        auto projected = IsometricView::Project({x, y, 0});
        double horizontal = projected.x;
        double vertical = projected.y / IsometricView::GroundDepth;
        if (std::abs(horizontal) > .0001 || std::abs(vertical) > .0001)
        {
            if (std::abs(horizontal) > std::abs(vertical) * 1.05)
            {
                facing = horizontal < 0 ? CharacterFacing::Left : CharacterFacing::Right;
            }
            else if (std::abs(vertical) > std::abs(horizontal) * 1.05)
            {
                facing = vertical < 0 ? CharacterFacing::Back : CharacterFacing::Front;
            }
            // Exact diagonals retain a compatible heading; otherwise prefer front/back.
            else if ((facing == CharacterFacing::Left && horizontal >= 0) ||
                     (facing == CharacterFacing::Right && horizontal <= 0) ||
                     (facing == CharacterFacing::Back && vertical >= 0) ||
                     (facing == CharacterFacing::Front && vertical <= 0))
            {
                facing = vertical < 0 ? CharacterFacing::Back : CharacterFacing::Front;
            }
        }
        stepPhase = moving ? travel / (sprinting ? RunStride : WalkStride) * Cycle : 0;
        // Retain the cycle through a stop and Walk/Run changes; no random world-time pose on restart.
        phase = std::fmod(phase + stepPhase, Cycle);
    }

    CharacterMotion Interpolated(double alpha) const
    {
        CharacterMotion result = *this;
        alpha = std::max(0.0, std::min(1.0, alpha));
        result.phase = std::fmod(phase - stepPhase * (1 - alpha), Cycle);
        if (result.phase < 0)
        {
            result.phase += Cycle;
        }
        return result;
    }

    double ClipPhase() const
    {
        return phase / Cycle;
    }

    unsigned Pose() const
    {
        return moving ? static_cast<unsigned>(ClipPhase() * 8) % 8 : IdlePose;
    }

    void Stop()
    {
        sprinting = false;
        moving = false;
        phase = 0;
        stepPhase = deltaX = deltaY = 0;
    }
};
