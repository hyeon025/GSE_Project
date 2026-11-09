#define NOMINMAX
#include <Windows.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include "../SimpleGame/GameWorld.h"

// Keep the pre-optimization formula independent of the cached road graph.
double ReferenceSettlementRoad(Game::Vec p)
{
    double distance = 1e10;
    for (int v = 0; v < Society::VillageCount; ++v)
    {
        Game::Vec c = Game::VillageCenter(v);
        if (Game::Distance(p, c) < Society::VillageRadius)
        {
            distance = std::min(distance, std::min(std::abs(p.x - c.x), std::abs(p.y - c.y)));
            distance = std::min(distance,
                std::min(std::abs(std::remainder(p.x - c.x, 112.0)), std::abs(std::remainder(p.y - c.y, 112.0))));
        }
        for (int other = v + 1; other < Society::VillageCount; ++other)
            if (Game::Distance(c, Game::VillageCenter(other)) <= Society::VillageSpacing * 1.01)
                distance = std::min(distance, Game::SegmentDistance(p, c, Game::VillageCenter(other)));
    }
    return distance;
}

void Check(Game::Vec p)
{
    double expected = ReferenceSettlementRoad(p);
    double actual = Game::SettlementRoad(p);
    assert(actual == expected); // No tolerance: minimap, clearing and collision thresholds must agree exactly.
    float reference = (float)std::min(expected, std::min(std::abs(p.x - p.y) * .7071,
        std::min(std::abs(p.x + p.y + 128) * .7071, std::abs(p.y - std::sin(p.x / 145) * 100 + 320) * .78)));
    assert(Game::Road(p) == reference);
}

int main()
{
    // Distant coordinates and dense settlement/link/minimap samples.
    for (int y = -3000; y <= 3000; y += 29)
        for (int x = -3000; x <= 3000; x += 31)
            Check({x + .1234567, y - .7654321});
    for (int v = 0; v < Society::VillageCount; ++v)
    {
        auto c = Game::VillageCenter(v);
        for (double side : {-1.0, 1.0})
        {
            // Preserve strict inequalities at the exact village boundary and adjacent floating-point values.
            double edge = c.x + side * Society::VillageRadius;
            Check({edge, c.y});
            Check({std::nextafter(edge, c.x), c.y});
            Check({std::nextafter(edge, side * 1e10), c.y});
        }
        for (int iy = 0; iy < 48; ++iy)
            for (int ix = 0; ix < 48; ++ix)
            {
                auto ground = IsometricView::GroundFromScreen({(ix - 24) * 12.0, (iy - 24) * 12.0});
                Check({c.x + ground.x, c.y + ground.y});
            }
        for (int other = v + 1; other < Society::VillageCount; ++other)
        {
            auto b = Game::VillageCenter(other);
            for (double alpha : {0., .1, .5, .9, 1.})
                for (double offset : {-55., -40., -33., -.00001, 0., .00001, 33., 40., 55.})
                    Check({c.x + (b.x - c.x) * alpha + offset, c.y + (b.y - c.y) * alpha});
        }
    }
    std::cout << "Road sampling tests passed: exact equivalence across grid, boundaries, links and minimap samples\n";
}
