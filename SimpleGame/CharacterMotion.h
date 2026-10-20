#pragma once
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
    bool moving = false;
    bool sprinting = false;
    static constexpr unsigned IdlePose = 8;

    void Update(double inputX, double inputY, double deltaX, double deltaY)
    {
        double travel = std::hypot(deltaX, deltaY);
        moving = travel > .001;
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
        phase = moving ? std::fmod(phase + travel * .17, 6.283185307179586) : 0;
    }

    unsigned Pose() const
    {
        return moving ? static_cast<unsigned>(phase / 6.283185307179586 * 8) % 8 : IdlePose;
    }

    void Stop()
    {
        sprinting = false;
        moving = false;
        phase = 0;
    }
};
