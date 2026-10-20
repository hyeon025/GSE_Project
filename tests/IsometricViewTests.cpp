#include <cassert>
#include <cmath>
#include "../SimpleGame/IsometricView.h"

int main()
{
    using namespace IsometricView;
    for (double x : {-10000.0, -32.0, 0.0, 64.0, 10000.0})
    {
        for (double y : {-9000.0, -32.0, 0.0, 96.0, 9000.0})
        {
            auto screen = Project({x, y, 0});
            auto ground = GroundFromScreen(screen);
            assert(std::abs(ground.x - x) < .00001);
            assert(std::abs(ground.y - y) < .00001);
            auto raised = Project({x, y, 64});
            assert(raised.x == screen.x);
            assert(std::abs(raised.y - screen.y + 64 * HeightScale) < .00001);
            for (double zoom : {.8, 1.15, 2.0})
            {
                for (double height : {720.0, 900.0, 1440.0})
                {
                    double pixelX = 720 + screen.x * zoom;
                    double pixelY = height * AnchorY + screen.y * zoom;
                    auto recovered = GroundFromScreen({(pixelX - 720) / zoom, (pixelY - height * AnchorY) / zoom});
                    assert(std::abs(recovered.x - x) < .00001);
                    assert(std::abs(recovered.y - y) < .00001);
                }
            }
        }
    }
    auto east = Project({1, 0, 0}), north = Project({0, 1, 0});
    assert(east.x > 0 && north.x < 0 && east.y < 0 && north.y < 0);
    assert(std::abs(east.x * north.x + east.y * north.y / (GroundDepth * GroundDepth)) < .00001);
    // Both connecting roads must remain diagonal instead of cancelling the camera rotation.
    for (Point road : {Point{1, 1}, Point{1, -1}})
    {
        auto p = Project({road.x, road.y, 0});
        assert(std::abs(p.x) > .25 && std::abs(p.y) > .25);
    }
    for (Point input :
         {Point{0, -1}, Point{0, 1}, Point{-1, 0}, Point{1, 0}, Point{-1, -1}, Point{1, -1}, Point{-1, 1}, Point{1, 1}})
    {
        auto movement = GroundFromScreen(input);
        auto p = Project({movement.x, movement.y, 0});
        assert(std::abs(p.x - input.x) < .00001 && std::abs(p.y - input.y) < .00001);
    }
    for (Point half : {Point{25, 23}, Point{20, 6}, Point{24, 24}})
    {
        auto bounds = GroundHalfExtents(half);
        auto front = Project({-half.x, -half.y, 0});
        assert(std::abs(front.y - bounds.y) < .00001);
        for (double x : {-half.x, half.x})
        {
            for (double y : {-half.y, half.y})
            {
                auto p = Project({x, y, 0});
                assert(std::abs(p.x) <= bounds.x + .00001 && std::abs(p.y) <= bounds.y + .00001);
            }
        }
    }
    assert(OccluderOpacity({0, 60}, 56, 130, 31) < .3f);
    assert(OccluderOpacity({100, 60}, 56, 130, 31) == 1);
    assert(OccluderOpacity({0, -40}, 56, 130, 31) == 1);
    assert(OccluderOpacity({0, 150}, 56, 130, 31) == 1);
    float edge = OccluderOpacity({62, 60}, 56, 130, 31);
    assert(edge > .3f && edge < 1);
}
