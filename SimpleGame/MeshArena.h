#pragma once
#include <cstddef>
#include <map>
#include <iterator>

// Vertex ranges in a bounded GPU buffer. Free ranges coalesce in either order.
class MeshArena
{
public:
    void Reset(size_t capacity)
    {
        free.clear();
        if (capacity) free.emplace(0, capacity);
    }
    bool Allocate(size_t count, size_t& offset)
    {
        if (!count) return false;
        for (auto it = free.begin(); it != free.end(); ++it)
        {
            if (it->second < count) continue;
            offset = it->first;
            const size_t remaining = it->second - count;
            free.erase(it);
            if (remaining) free.emplace(offset + count, remaining);
            return true;
        }
        return false;
    }
    void Release(size_t offset, size_t count)
    {
        auto next = free.lower_bound(offset);
        if (next != free.begin())
        {
            auto previous = std::prev(next);
            if (previous->first + previous->second == offset)
            {
                offset = previous->first;
                count += previous->second;
                free.erase(previous);
            }
        }
        if (next != free.end() && offset + count == next->first)
        {
            count += next->second;
            free.erase(next);
        }
        free.emplace(offset, count);
    }
private:
    std::map<size_t, size_t> free;
};
