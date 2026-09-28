#include <cassert>
#include <limits>
#include <sstream>
#include "../SimpleGame/RenderMeshCache.h"
#include "../SimpleGame/RenderMetrics.h"

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
}
