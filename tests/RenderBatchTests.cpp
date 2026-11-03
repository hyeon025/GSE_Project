#include <cassert>
#include <algorithm>
#include <limits>
#include <sstream>
#include "../SimpleGame/RenderMeshCache.h"
#include "../SimpleGame/RenderMetrics.h"
#include "../SimpleGame/MeshArena.h"
#include <random>

void CheckMeshArena()
{
    MeshArena arena;
    arena.Reset(100);
    size_t a, b, c, offset;
    assert(!arena.Allocate(0, offset) && !arena.Allocate(101, offset));
    assert(arena.Allocate(30, a) && arena.Allocate(40, b) && arena.Allocate(30, c));
    assert(a == 0 && b == 30 && c == 70 && !arena.Allocate(1, offset));
    arena.Release(a, 30);
    arena.Release(c, 30);
    assert(!arena.Allocate(40, offset)); // fragmented free space cannot overlap the live middle
    arena.Release(b, 40);
    assert(arena.Allocate(100, offset) && offset == 0);
    arena.Reset(257);
    std::mt19937 random(37);
    std::vector<std::pair<size_t, size_t>> live;
    for (int i = 0; i < 10000; ++i)
    {
        if (!live.empty() && random() % 2)
        {
            size_t n = random() % live.size();
            arena.Release(live[n].first, live[n].second);
            live.erase(live.begin() + n);
        }
        else
        {
            size_t count = random() % 61 + 1;
            if (arena.Allocate(count, offset))
            {
                assert(offset + count <= 257);
                for (const auto& range : live)
                    assert(offset + count <= range.first || range.first + range.second <= offset);
                live.push_back({offset, count});
            }
        }
    }
    for (const auto& range : live) arena.Release(range.first, range.second);
    assert(arena.Allocate(257, offset) && offset == 0);
}

void CheckMeshRoundTrip()
{
    using namespace RenderMeshCache;
    const std::vector<Vertex> original = {{0, 0, 1, 0, 0, 1, 0, 0}, {1, 0, 0, 1, 0, 1, 0, 0}, {0, 1, 0, 0, 1, 1, 0, 0}};
    std::ostringstream output(std::ios::binary);
    assert(Save(output, 4, 91, original));
    const auto bytes = output.str();
    std::vector<Vertex> loaded;
    std::istringstream good(bytes, std::ios::binary);
    assert(Load(good, 4, 91, loaded));
    assert(loaded.size() == 3 && Checksum(loaded) == Checksum(original));
    std::istringstream wrongKey(bytes, std::ios::binary);
    assert(!Load(wrongKey, 4, 92, loaded));
    std::istringstream wrongKind(bytes, std::ios::binary);
    assert(!Load(wrongKind, 5, 91, loaded));
    std::istringstream truncated(bytes.substr(0, bytes.size() - 1), std::ios::binary);
    assert(!Load(truncated, 4, 91, loaded));
    std::string corrupt = bytes;
    corrupt.back() ^= 1;
    std::istringstream damaged(corrupt, std::ios::binary);
    assert(!Load(damaged, 4, 91, loaded));
    corrupt = bytes;
    corrupt[8] ^= 1; // Cache revision, not vertex payload.
    std::istringstream oldRevision(corrupt, std::ios::binary);
    assert(!Load(oldRevision, 4, 91, loaded));
    std::istringstream extra(bytes + "extra", std::ios::binary);
    assert(!Load(extra, 4, 91, loaded));
    assert(Checksum(loaded) == Checksum(original)); // Failed loads cannot mutate the caller.
    auto invalid = original;
    invalid.front().x = std::numeric_limits<float>::infinity();
    assert(!Save(output, 4, 91, invalid));
    assert(!Valid({}) && !Valid(std::vector<Vertex>(4)));
    assert(!Valid(std::vector<Vertex>(MaxVertices + 1)));
}

void CheckBatchBoundsAndCounters()
{
    assert(!RenderBatchLimits::ExceedsPadding(102, 96, 2));
    assert(!RenderBatchLimits::ExceedsPadding(96, 96, 8));
    assert(RenderBatchLimits::ExceedsPadding(96, 96, 9));
    assert(!RenderBatchLimits::ExceedsPadding(6000, 6000, 2));
    assert(RenderBatchLimits::ExceedsPadding(6003, 6000, 3));
    // A detailed body followed by 100 tiny tufts cannot incur >2x shader work.
    std::uint64_t useful = 0, maximum = 0, submitted = 0, allUseful = 0;
    size_t count = 0;
    for (int group = 0; group < 32; ++group)
    {
        for (int item = 0; item < 101; ++item)
        {
            std::uint64_t vertices = item == 0 ? 6000 : 3;
            if (count && RenderBatchLimits::ExceedsPadding(useful + vertices, std::max(maximum, vertices), count + 1))
            {
                submitted += maximum * count;
                allUseful += useful;
                useful = maximum = count = 0;
            }
            useful += vertices;
            maximum = std::max(maximum, vertices);
            ++count;
        }
    }
    submitted += maximum * count;
    allUseful += useful;
    assert(submitted <= allUseful * 2);
    assert(RenderBatchLimits::Instances == 2048);
    RenderFrameStats stats;
    stats.Add(RenderCounter::DrawArrays);
    stats.Add(RenderCounter::DrawInstanced, 2);
    stats.Add(RenderCounter::Instances, 20);
    assert(stats.Get(RenderCounter::DrawArrays) == 1);
    assert(stats.Get(RenderCounter::DrawInstanced) == 2);
    assert(stats.Get(RenderCounter::Instances) == 20);
    stats = {};
    assert(stats.Get(RenderCounter::DrawInstanced) == 0);
}

int main()
{
    CheckMeshRoundTrip();
    CheckBatchBoundsAndCounters();
    CheckMeshArena();
}
