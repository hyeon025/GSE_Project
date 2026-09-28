#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

using ActorId = std::uint64_t;

struct ActorKey
{
    int kind = 0;
    std::int64_t first = 0, second = 0;

    bool operator==(const ActorKey& other) const
    {
        return kind == other.kind && first == other.first && second == other.second;
    }
};

struct ActorTransform
{
    double x = 0, y = 0, height = 0;
    double rotation = 0, scale = 1;

    static ActorTransform Compose(const ActorTransform& parent, const ActorTransform& local)
    {
        double c = std::cos(parent.rotation), s = std::sin(parent.rotation);
        return {
            parent.x + (local.x * c - local.y * s) * parent.scale,
            parent.y + (local.x * s + local.y * c) * parent.scale,
            parent.height + local.height * parent.scale,
            parent.rotation + local.rotation,
            parent.scale * local.scale
        };
    }

    static ActorTransform RelativeTo(const ActorTransform& world, const ActorTransform& parent)
    {
        double dx = (world.x - parent.x) / parent.scale, dy = (world.y - parent.y) / parent.scale;
        double c = std::cos(parent.rotation), s = std::sin(parent.rotation);
        return {
            dx * c + dy * s,
            -dx * s + dy * c,
            (world.height - parent.height) / parent.scale,
            world.rotation - parent.rotation,
            world.scale / parent.scale
        };
    }
};

enum class ActorLayer
{
    Background,
    Ground,
    Objects,
    Effects,
    Interface
};

class SceneGraph;

class Actor
{
public:
    virtual ~Actor() = default;
    Actor() = default;
    Actor(const Actor&) = delete;
    Actor& operator=(const Actor&) = delete;

    ActorId Id() const
    {
        return m_Id;
    }

    ActorId Parent() const
    {
        return m_Parent;
    }

    const std::vector<ActorId>& Children() const
    {
        return m_Children;
    }

    const ActorTransform& LocalTransform() const
    {
        return m_Local;
    }

    void SetLocalTransform(const ActorTransform& value)
    {
        if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.height) ||
            !std::isfinite(value.rotation) || !std::isfinite(value.scale) || value.scale <= 0)
        {
            throw std::invalid_argument("Actor transform must be finite with positive scale.");
        }
        m_Local = value;
    }

    bool visible = true;
    bool enabled = true;
    bool renderable = true;
    bool streamed = false;
    ActorLayer layer = ActorLayer::Objects;
    int order = 0;
    double depthOffset = 0;

protected:
    virtual void OnUpdate(float)
    {
    }

    virtual void OnDraw(const ActorTransform&)
    {
    }

private:
    friend class SceneGraph;
    ActorId m_Id = 0, m_Parent = 0;
    ActorKey m_Key;
    ActorTransform m_Local;
    std::vector<ActorId> m_Children;
    std::uint64_t m_Seen = 0;
    bool m_Destroyed = false;
};

// A node for a scene-level system or a screen-space HUD, not an entity container.
class CallbackActor : public Actor
{
public:
    std::function<void(float)> update;
    std::function<void(const ActorTransform&)> draw;

protected:
    void OnUpdate(float dt) override
    {
        if (update)
        {
            update(dt);
        }
    }

    void OnDraw(const ActorTransform& transform) override
    {
        if (draw)
        {
            draw(transform);
        }
    }
};

// Presentation owns a snapshot, never a pointer into an erasable gameplay vector.
template <class Data> class VisualActor : public Actor
{
public:
    using Draw = std::function<void(const Data&, const ActorTransform&)>;

    explicit VisualActor(Draw draw)
        : m_Draw(std::move(draw))
    {
    }

    Data data = {};

protected:
    void OnDraw(const ActorTransform& transform) override
    {
        m_Draw(data, transform);
    }

private:
    Draw m_Draw;
};
