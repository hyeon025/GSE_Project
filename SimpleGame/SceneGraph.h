#pragma once
#include "Actor.h"
#include <algorithm>
#include <memory>
#include <unordered_map>

class SceneGraph
{
public:
    SceneGraph() = default;
    SceneGraph(const SceneGraph&) = delete;
    SceneGraph& operator=(const SceneGraph&) = delete;

    Actor* Find(ActorId id) const
    {
        auto it = m_Actors.find(id);
        return it == m_Actors.end() || it->second->m_Destroyed ? nullptr : it->second.get();
    }

    Actor* Find(ActorKey key) const
    {
        auto it = m_Keys.find(key);
        return it == m_Keys.end() ? nullptr : Find(it->second);
    }

    template <class T, class... Args> T& GetOrCreate(ActorKey key, ActorId parent, Args&&... args)
    {
        auto it = m_Keys.find(key);
        if (it != m_Keys.end())
        {
            auto* actor = dynamic_cast<T*>(Find(it->second));
            if (!actor)
            {
                throw std::logic_error("Actor key has another type or is pending destruction.");
            }
            if (actor->Parent() != parent)
            {
                Reparent(actor->Id(), parent, false);
            }
            Touch(*actor);
            return *actor;
        }
        if (parent && !Find(parent))
        {
            throw std::invalid_argument("Missing actor parent.");
        }
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        T& actor = *node;
        actor.m_Id = ++m_NextId;
        actor.m_Parent = parent;
        actor.m_Key = key;
        m_Actors.emplace(actor.Id(), std::move(node));
        m_Keys.emplace(key, actor.Id());
        Children(parent).push_back(actor.Id());
        Touch(actor);
        return actor;
    }

    Actor& Group(ActorKey key, ActorId parent = 0)
    {
        auto& group = GetOrCreate<Actor>(key, parent);
        group.renderable = false;
        return group;
    }

    ActorTransform WorldTransform(ActorId id) const
    {
        const Actor* node = Find(id);
        if (!node)
        {
            throw std::invalid_argument("Missing actor.");
        }
        ActorTransform result = node->m_Local;
        while (node->Parent())
        {
            node = Find(node->Parent());
            result = ActorTransform::Compose(node->m_Local, result);
        }
        return result;
    }

    void Reparent(ActorId id, ActorId parent, bool keepWorld = true)
    {
        RequireIdle();
        Actor* node = Find(id);
        if (!node || (parent && !Find(parent)))
        {
            throw std::invalid_argument("Missing actor.");
        }
        for (Actor* ancestor = Find(parent); ancestor; ancestor = Find(ancestor->Parent()))
        {
            if (ancestor->Id() == id)
            {
                throw std::invalid_argument("Scene graph cycle.");
            }
        }
        auto local = node->m_Local;
        if (keepWorld)
        {
            auto pose = WorldTransform(id);
            local = parent ? ActorTransform::RelativeTo(pose, WorldTransform(parent)) : pose;
        }
        auto& siblings = Children(node->Parent());
        siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
        Children(parent).push_back(id);
        node->m_Parent = parent;
        node->SetLocalTransform(local);
    }

    void Destroy(ActorId id)
    {
        if (Actor* node = Find(id))
        {
            MarkDestroyed(*node);
        }
        if (!m_Traversing)
        {
            CollectDestroyed();
        }
    }

    void Clear()
    {
        RequireIdle();
        m_Actors.clear();
        m_Keys.clear();
        m_Roots.clear();
        m_Queue.clear();
        m_HasDestroyed = false;
        // IDs are never reused, including after a level restart.
    }

    size_t Size() const
    {
        return m_Actors.size();
    }

    void BeginSync()
    {
        RequireIdle();
        ++m_Generation;
    }

    void EndSync()
    {
        RequireIdle();
        for (auto& item : m_Actors)
        {
            Actor& node = *item.second;
            if (node.streamed && node.m_Seen != m_Generation && !node.m_Destroyed)
            {
                MarkDestroyed(node);
            }
        }
        CollectDestroyed();
    }

    void Update(float dt)
    {
        Traverse(
            [dt](Actor& actor, const Entry& entry)
            {
                if (entry.enabled)
                {
                    actor.OnUpdate(dt);
                }
            },
            false,
            {}
        );
    }

    using Depth = std::function<double(const ActorTransform&)>;

    void Render(const Depth& depth = {})
    {
        Traverse(
            [](Actor& actor, const Entry& entry)
            {
                if (entry.visible && actor.renderable)
                {
                    actor.OnDraw(entry.world);
                }
            },
            true,
            depth
        );
    }

private:
    struct KeyHash
    {
        size_t operator()(const ActorKey& key) const
        {
            size_t h = std::hash<int>{}(key.kind);
            h ^= std::hash<std::int64_t>{}(key.first) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h ^ (std::hash<std::int64_t>{}(key.second) + 0x9e3779b9 + (h << 6) + (h >> 2));
        }
    };

    struct Entry
    {
        ActorId id;
        ActorTransform world;
        bool enabled, visible;
        ActorLayer layer;
        int order;
        double depth;
    };

    std::unordered_map<ActorId, std::unique_ptr<Actor>> m_Actors;
    std::unordered_map<ActorKey, ActorId, KeyHash> m_Keys;
    std::vector<ActorId> m_Roots;
    std::vector<Entry> m_Queue;
    ActorId m_NextId = 0;
    std::uint64_t m_Generation = 0;
    bool m_Traversing = false;
    bool m_HasDestroyed = false;

    void RequireIdle() const
    {
        if (m_Traversing)
        {
            throw std::logic_error("Cannot restructure a traversing scene graph.");
        }
    }

    std::vector<ActorId>& Children(ActorId parent)
    {
        return parent ? m_Actors.at(parent)->m_Children : m_Roots;
    }

    void Touch(Actor& actor)
    {
        for (Actor* node = &actor; node; node = Find(node->Parent()))
        {
            node->m_Seen = m_Generation;
        }
    }

    void MarkDestroyed(Actor& node)
    {
        m_HasDestroyed = true;
        node.m_Destroyed = true;
        for (ActorId id : node.m_Children)
        {
            MarkDestroyed(*m_Actors.at(id));
        }
    }

    void CollectDestroyed()
    {
        if (!m_HasDestroyed)
        {
            return;
        }
        for (auto& item : m_Actors)
        {
            auto& children = item.second->m_Children;
            children.erase(
                std::remove_if(
                    children.begin(),
                    children.end(),
                    [this](ActorId id)
                    {
                        return m_Actors.at(id)->m_Destroyed;
                    }
                ),
                children.end()
            );
        }
        m_Roots.erase(
            std::remove_if(
                m_Roots.begin(),
                m_Roots.end(),
                [this](ActorId id)
                {
                    return m_Actors.at(id)->m_Destroyed;
                }
            ),
            m_Roots.end()
        );
        for (auto it = m_Actors.begin(); it != m_Actors.end();)
        {
            if (it->second->m_Destroyed)
            {
                m_Keys.erase(it->second->m_Key);
                it = m_Actors.erase(it);
            }
            else
            {
                ++it;
            }
        }
        m_HasDestroyed = false;
    }

    void Gather(ActorId id, const ActorTransform& parent, bool enabled, bool visible, const Depth& depth)
    {
        Actor* actor = Find(id);
        if (!actor)
        {
            return;
        }
        auto world = ActorTransform::Compose(parent, actor->m_Local);
        enabled = enabled && actor->enabled;
        visible = visible && actor->visible;
        m_Queue.push_back(
            {id,
             world,
             enabled,
             visible,
             actor->layer,
             actor->order,
             (depth ? depth(world) : world.y) + actor->depthOffset * world.scale}
        );
        for (ActorId child : actor->m_Children)
        {
            Gather(child, world, enabled, visible, depth);
        }
    }

    template <class Callback> void Traverse(Callback callback, bool draw, const Depth& depth)
    {
        RequireIdle();
        m_Queue.clear();
        for (ActorId root : m_Roots)
        {
            Gather(root, {}, true, true, depth);
        }
        if (draw)
        {
            std::sort(
                m_Queue.begin(),
                m_Queue.end(),
                [](const Entry& a, const Entry& b)
                {
                    if (a.layer != b.layer)
                    {
                        return a.layer < b.layer;
                    }
                    if (a.order != b.order)
                    {
                        return a.order < b.order;
                    }
                    if (a.depth != b.depth)
                    {
                        return a.depth < b.depth;
                    }
                    return a.id < b.id;
                }
            );
        }
        m_Traversing = true;
        try
        {
            for (const auto& entry : m_Queue)
            {
                if (Actor* actor = Find(entry.id))
                {
                    callback(*actor, entry);
                }
            }
        }
        catch (...)
        {
            m_Traversing = false;
            CollectDestroyed();
            throw;
        }
        m_Traversing = false;
        CollectDestroyed();
    }
};
