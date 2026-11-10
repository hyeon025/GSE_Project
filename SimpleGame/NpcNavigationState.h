#pragma once
#include <array>
#include <vector>

namespace Society
{
    // Session-local routes are rebuilt on load. No pointers into the NPC vector.
    struct Navigation
    {
        std::vector<std::array<double, 2>> points;
        size_t next = 0;
        bool valid = false, checked = false;
        double goalX = 0, goalY = 0, retry = 0;
        int routeVillage = -1, failures = 0;
    };
    struct Routine
    {
        double workRemaining = -1, leisureRemaining = 0;
        double leisureX = 0, leisureY = 0, taskX = 0, taskY = 0;
        bool strolling = false;
        int taskActivity = -1;
        unsigned sequence = 0;
    };
}
