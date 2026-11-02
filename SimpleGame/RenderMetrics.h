#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

enum class RenderCounter
{
    DrawArrays,
    DrawInstanced,
    WorldDraws,
    PostDraws,
    UiDraws,
    Instances,
    UsefulVertices,
    SubmittedVertices,
    UploadBytes,
    MeshHits,
    MeshMisses,
    MeshBuilds,
    DiskHits,
    DiskMisses,
    DiskRejected,
    DiskWrites,
    DiskWriteErrors,
    MeshEvictions,
    TextHits,
    TextMisses,
    TextureUploadBytes,
    FlushExplicit,
    FlushPass,
    FlushTexture,
    FlushCapacity,
    FlushPadding,
    FlushCache,
    EmptyFlushes,
    MeshBuildUs,
    CacheIoUs,
    Count
};

enum class BatchBreak
{
    Explicit,
    Pass,
    Texture,
    Capacity,
    Padding,
    Cache
};

struct RenderFrameStats
{
    std::array<std::uint64_t, static_cast<size_t>(RenderCounter::Count)> counters = {};

    void Add(RenderCounter counter, std::uint64_t value = 1)
    {
        counters[static_cast<size_t>(counter)] += value;
    }

    std::uint64_t Get(RenderCounter counter) const
    {
        return counters[static_cast<size_t>(counter)];
    }
};

struct RenderBatchLimits
{
    static constexpr size_t Instances = 2048;
    static constexpr std::uint64_t PaddingRatio = 8;
    static constexpr std::uint64_t LargeMeshPaddingRatio = 2;
    static constexpr std::uint64_t LargeMeshVertices = 1024;

    static bool ExceedsPadding(std::uint64_t useful, std::uint64_t maximum, size_t count)
    {
        // One detailed character must not make thousands of tiny grass/UI instances
        // execute the character's full vertex count. Small primitive batches stay broad.
        const auto ratio = maximum > LargeMeshVertices ? LargeMeshPaddingRatio : PaddingRatio;
        return maximum * count > useful * ratio;
    }
};
