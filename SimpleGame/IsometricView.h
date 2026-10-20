#pragma once
#include <algorithm>
#include <cmath>

namespace IsometricView
{
    // 22.5-degree azimuth keeps both world axes and the x=y village road diagonal.
    constexpr double AxisX = 1.3065629648763766;
    constexpr double AxisY = .5411961001461970;
    constexpr float GroundDepth = .65f;
    constexpr float HeightScale = 1.0747093f;
    constexpr float AnchorY = .52f;
    constexpr float DefaultZoom = 1.15f;
    constexpr unsigned MeshRevision = 3;

    struct Point
    {
        double x, y;
    };

    struct Vertex
    {
        double x, y, z;
    };

    inline Point Project(Vertex p)
    {
        return {p.x * AxisX - p.y * AxisY, -(p.x * AxisY + p.y * AxisX) * GroundDepth - p.z * HeightScale};
    }

    inline Point GroundFromScreen(Point p)
    {
        constexpr double determinant = AxisX * AxisX + AxisY * AxisY;
        return {
            (p.x * AxisX - p.y * AxisY / GroundDepth) / determinant,
            (-p.x * AxisY - p.y * AxisX / GroundDepth) / determinant
        };
    }

    inline Point GroundHalfExtents(Point halfSize)
    {
        return {halfSize.x * AxisX + halfSize.y * AxisY, (halfSize.x * AxisY + halfSize.y * AxisX) * GroundDepth};
    }

    inline float SmoothUnit(double value)
    {
        float t = static_cast<float>(std::max(0.0, std::min(1.0, value)));
        return t * t * (3 - 2 * t);
    }

    inline float OccluderOpacity(Point relative, float halfWidth, float above, float below)
    {
        // Bounds and relative position are unzoomed screen units, measured from the player's feet.
        float horizontal = SmoothUnit((halfWidth + 12 - std::abs(relative.x)) / 12);
        float foreground = SmoothUnit((relative.y + below) / 10);
        float overlap = SmoothUnit((above - relative.y) / 16);
        return 1 - .72f * horizontal * foreground * overlap;
    }
}
