#include <cassert>
#include <set>
#include "../SimpleGame/CharacterMesh.h"

void CheckDirections()
{
    CharacterMotion motion;
    motion.Update(1, 1, 1, 1);
    assert(motion.facing == CharacterFacing::Back && motion.moving);
    motion.Update(-1, 1, -1, 1);
    assert(motion.facing == CharacterFacing::Left);
    motion.Update(-1, -1, -1, -1);
    assert(motion.facing == CharacterFacing::Front);
    motion.Update(1, -1, 1, -1);
    assert(motion.facing == CharacterFacing::Right);
    motion.Update(0, 0, 0, 0);
    assert(motion.facing == CharacterFacing::Right && !motion.moving);
    assert(motion.Pose() == CharacterMotion::IdlePose);
    motion.Update(1, 1, 0, 0);
    assert(motion.facing == CharacterFacing::Back && !motion.moving);
    motion.Update(1, -1, -1, 1);
    assert(motion.facing == CharacterFacing::Left); // Collision sliding follows actual displacement.
    for (int i = 0; i < 200; ++i)
    {
        motion.Update(1, 1, 3, 3);
        assert(motion.Pose() < 8 && motion.phase >= 0 && motion.phase < 6.283185307179586);
    }
    motion.Stop();
    assert(motion.Pose() == CharacterMotion::IdlePose && motion.facing == CharacterFacing::Back);
    const IsometricView::Point inputs[] = {{0, 1}, {0, -1}, {-1, 0}, {1, 0}};
    const CharacterFacing expected[] =
        {CharacterFacing::Front, CharacterFacing::Back, CharacterFacing::Left, CharacterFacing::Right};
    for (int i = 0; i < 4; ++i)
    {
        auto movement = IsometricView::GroundFromScreen(inputs[i]);
        motion.Update(movement.x, movement.y, movement.x, movement.y);
        assert(motion.facing == expected[i]);
        motion.Update(movement.x, movement.y, 0, 0);
        assert(motion.facing == expected[i] && !motion.moving);
    }
}

void CheckCachedPoses()
{
    std::set<std::uint64_t> keys;
    for (unsigned facing = 0; facing < 4; ++facing)
    {
        for (unsigned pose = 0; pose <= CharacterMotion::IdlePose; ++pose)
        {
            CharacterMesh::Appearance appearance;
            appearance.player = true;
            appearance.crossbow = true;
            appearance.facing = static_cast<CharacterFacing>(facing);
            appearance.pose = pose;
            assert(keys.insert(CharacterMesh::Key(appearance)).second);
            auto vertices = CharacterMesh::Build(appearance);
            assert(RenderMeshCache::Valid(vertices));
            assert(vertices.size() < 12000);
            for (const auto& vertex : vertices)
            {
                assert(vertex.x > -25 && vertex.x < 25);
                assert(vertex.y > -65 && vertex.y < 10);
                assert(vertex.a == 1);
            }
        }
    }
    CharacterMesh::Appearance front;
    auto first = CharacterMesh::Build(front);
    auto again = CharacterMesh::Build(front);
    assert(RenderMeshCache::Checksum(first) == RenderMeshCache::Checksum(again));
    front.facing = CharacterFacing::Back;
    auto back = CharacterMesh::Build(front);
    assert(RenderMeshCache::Checksum(first) != RenderMeshCache::Checksum(back));
}

void CheckLocomotionPhase()
{
    CharacterMotion whole, divided;
    whole.Update(1, 0, 17, 0);
    for (int i = 0; i < 10; ++i)
    {
        divided.Update(1, 0, 1.7, 0);
    }
    assert(std::abs(whole.phase - divided.phase) < 1e-9);
    assert(std::abs(whole.ClipPhase() - .2) < 1e-9);
    double held = whole.phase;
    whole.Update(1, 0, 0, 0);
    assert(!whole.moving && whole.phase == held && whole.stepPhase == 0);
    assert(whole.deltaX == 0 && whole.deltaY == 0);
    whole.Update(1, 0, 9.2, 0, true);
    assert(whole.sprinting && std::abs(whole.ClipPhase() - .3) < 1e-9);
    // Interpolating a wrapped cycle must take the short forward path.
    whole.phase = CharacterMotion::Cycle * .95;
    whole.Update(1, 0, 8.5, 0);
    assert(std::abs(whole.Interpolated(0).ClipPhase() - .95) < 1e-9);
    assert(std::abs(whole.Interpolated(.75).ClipPhase() - .025) < 1e-9);
    assert(std::abs(whole.Interpolated(1).ClipPhase() - .05) < 1e-9);
    assert(!whole.sprinting);
    whole.Stop();
    assert(whole.phase == 0 && whole.stepPhase == 0 && whole.deltaX == 0 && whole.deltaY == 0);
}

int main()
{
    CheckDirections();
    CheckCachedPoses();
    CheckLocomotionPhase();
}
