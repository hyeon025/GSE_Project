#pragma once
#include <cmath>
#include <cstdint>
#include <istream>
#include <ostream>
#include <type_traits>
#include <vector>

namespace RenderMeshCache
{
    struct Vertex
    {
        float x, y, r, g, b, a, u, v;
    };

    static_assert(sizeof(Vertex) == 32 && std::is_trivially_copyable<Vertex>::value, "Vertex cache layout changed.");
    // Bump when vertex layout, primitive tessellation or mesh builders change.
    constexpr std::uint32_t Revision = 2;
    constexpr std::uint32_t MaxVertices = 65536;

    inline std::uint64_t Checksum(const std::vector<Vertex>& vertices)
    {
        std::uint64_t value = 14695981039346656037ull;
        const auto* bytes = reinterpret_cast<const unsigned char*>(vertices.data());
        for (size_t i = 0; i < vertices.size() * sizeof(Vertex); ++i)
        {
            value = (value ^ bytes[i]) * 1099511628211ull;
        }
        return value;
    }

    template <class T> bool ReadValue(std::istream& in, T& value)
    {
        return static_cast<bool>(in.read(reinterpret_cast<char*>(&value), sizeof(value)));
    }

    template <class T> void WriteValue(std::ostream& out, const T& value)
    {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    inline bool Valid(const std::vector<Vertex>& vertices)
    {
        if (vertices.empty() || vertices.size() > MaxVertices || vertices.size() % 3)
        {
            return false;
        }
        for (const auto& v : vertices)
        {
            const float values[] = {v.x, v.y, v.r, v.g, v.b, v.a, v.u, v.v};
            for (float value : values)
            {
                if (!std::isfinite(value))
                {
                    return false;
                }
            }
        }
        return true;
    }

    inline bool Load(std::istream& in, std::uint32_t kind, std::uint64_t variant, std::vector<Vertex>& vertices)
    {
        std::uint32_t magic, endian, revision, storedKind, stride, count;
        std::uint64_t storedVariant, checksum;
        if (!ReadValue(in, magic) || !ReadValue(in, endian) || !ReadValue(in, revision) || !ReadValue(in, storedKind) ||
            !ReadValue(in, storedVariant) || !ReadValue(in, stride) || !ReadValue(in, count) ||
            !ReadValue(in, checksum) || magic != 0x4753454d || endian != 0x01020304 || revision != Revision ||
            storedKind != kind || storedVariant != variant || stride != sizeof(Vertex) || !count ||
            count > MaxVertices || count % 3)
        {
            return false;
        }
        std::vector<Vertex> loaded(count);
        if (!in.read(reinterpret_cast<char*>(loaded.data()), count * sizeof(Vertex)) ||
            in.peek() != std::char_traits<char>::eof() || !Valid(loaded) || Checksum(loaded) != checksum)
        {
            return false;
        }
        vertices.swap(loaded);
        return true;
    }

    inline bool Save(std::ostream& out, std::uint32_t kind, std::uint64_t variant, const std::vector<Vertex>& vertices)
    {
        if (!Valid(vertices))
        {
            return false;
        }
        WriteValue(out, std::uint32_t(0x4753454d));
        WriteValue(out, std::uint32_t(0x01020304));
        WriteValue(out, Revision);
        WriteValue(out, kind);
        WriteValue(out, variant);
        WriteValue(out, std::uint32_t(sizeof(Vertex)));
        WriteValue(out, std::uint32_t(vertices.size()));
        WriteValue(out, Checksum(vertices));
        out.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(Vertex));
        return static_cast<bool>(out);
    }
}
