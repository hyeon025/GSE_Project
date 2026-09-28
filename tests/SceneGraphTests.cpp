#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
#include "../SimpleGame/SceneGraph.h"
#include "../SimpleGame/CombatActors.h"

bool Near(double a, double b)
{
    return std::abs(a - b) < 0.000001;
}

void CheckTransformsAndParents()
{
    SceneGraph graph;
    auto& parent = graph.Group({1});
    parent.SetLocalTransform({10, 20, 3, 1.5707963267948966, 2});
    auto& child = graph.GetOrCreate<Actor>({2}, parent.Id());
    child.SetLocalTransform({4, 5, 6, .25, .5});
    auto world = graph.WorldTransform(child.Id());
    assert(Near(world.x, 0) && Near(world.y, 28) && Near(world.height, 15));
    assert(Near(world.rotation, 1.8207963267948966) && Near(world.scale, 1));
    auto& other = graph.Group({3});
    other.SetLocalTransform({-5, 9, 4, -.5, 3});
    graph.Reparent(child.Id(), other.Id());
    auto preserved = graph.WorldTransform(child.Id());
    assert(Near(preserved.x, world.x) && Near(preserved.y, world.y));
    assert(Near(preserved.height, world.height) && Near(preserved.scale, world.scale));
    assert(Near(preserved.rotation, world.rotation));
    assert(parent.Children().empty() && other.Children().size() == 1);
    bool rejected = false;
    try
    {
        graph.Reparent(other.Id(), child.Id());
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try
    {
        graph.Reparent(child.Id(), 9999);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    assert(rejected && child.Parent() == other.Id());
    rejected = false;
    try
    {
        child.SetLocalTransform({0, 0, 0, 0, 0});
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try
    {
        child.SetLocalTransform({std::numeric_limits<double>::infinity()});
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    assert(rejected);
    graph.Reparent(child.Id(), 0, false);
    assert(child.Parent() == 0 && other.Children().empty());
}

void CheckLayersVisibilityAndUpdates()
{
    SceneGraph graph;
    std::vector<int> rendered;
    int updates = 0;
    auto& parent = graph.Group({1});
    auto& near = graph.GetOrCreate<CallbackActor>({2}, parent.Id());
    near.SetLocalTransform({0, 30});
    near.draw = [&](const ActorTransform&)
    {
        rendered.push_back(2);
    };
    near.update = [&](float)
    {
        ++updates;
    };
    auto& far = graph.GetOrCreate<CallbackActor>({3}, 0);
    far.SetLocalTransform({0, 10});
    far.draw = [&](const ActorTransform&)
    {
        rendered.push_back(3);
    };
    auto& overlay = graph.GetOrCreate<CallbackActor>({4}, parent.Id());
    overlay.layer = ActorLayer::Interface;
    overlay.SetLocalTransform({0, -100});
    overlay.draw = [&](const ActorTransform&)
    {
        rendered.push_back(4);
    };
    graph.Render();
    assert((rendered == std::vector<int>{3, 2, 4}));
    rendered.clear();
    graph.Render(
        [](const ActorTransform& p)
        {
            return -p.y;
        }
    );
    assert((rendered == std::vector<int>{2, 3, 4}));
    parent.visible = false;
    rendered.clear();
    graph.Render();
    assert((rendered == std::vector<int>{3}));
    graph.Update(.01f);
    assert(updates == 1); // Visibility and simulation activation are independent.
    parent.enabled = false;
    graph.Update(.01f);
    assert(updates == 1);
    parent.visible = true;
    near.renderable = false;
    rendered.clear();
    graph.Render();
    assert((rendered == std::vector<int>{3, 4}));
}

void CheckReuseAndStreaming()
{
    SceneGraph graph;
    graph.BeginSync();
    auto& parent = graph.Group({1});
    parent.streamed = true;
    auto parentId = parent.Id();
    auto& child = graph.GetOrCreate<Actor>({2, 7, 8}, parentId);
    child.streamed = true;
    auto childId = child.Id();
    graph.EndSync();
    graph.BeginSync();
    auto& reused = graph.GetOrCreate<Actor>({2, 7, 8}, parentId);
    assert(&reused == &child && reused.Id() == childId);
    graph.EndSync(); // Touching a child also retains its streamed parent.
    assert(graph.Size() == 2 && graph.Find(parentId));
    bool rejected = false;
    try
    {
        graph.GetOrCreate<CallbackActor>({2, 7, 8}, parentId);
    }
    catch (const std::logic_error&)
    {
        rejected = true;
    }
    assert(rejected);
    graph.BeginSync();
    graph.EndSync();
    assert(graph.Size() == 0 && !graph.Find(childId));
    auto newer = graph.GetOrCreate<Actor>({2, 7, 8}, 0).Id();
    assert(newer > childId);
    graph.Clear();
    assert(graph.GetOrCreate<Actor>({2, 7, 8}, 0).Id() > newer);
}

void CheckDeferredMutation()
{
    SceneGraph graph;
    auto parentId = graph.Group({1}).Id();
    auto& destroyer = graph.GetOrCreate<CallbackActor>({2}, parentId);
    auto& victim = graph.GetOrCreate<CallbackActor>({3}, parentId);
    int victimUpdates = 0, newUpdates = 0;
    victim.update = [&](float)
    {
        ++victimUpdates;
    };
    destroyer.update = [&](float)
    {
        graph.Destroy(parentId);
        auto& next = graph.GetOrCreate<CallbackActor>({4}, 0);
        next.update = [&](float)
        {
            ++newUpdates;
        };
    };
    graph.Update(.01f);
    assert(!graph.Find(parentId) && graph.Size() == 1);
    assert(victimUpdates == 0 && newUpdates == 0);
    graph.Update(.01f);
    assert(newUpdates == 1);
    auto* next = graph.Find(ActorKey{4});
    auto nextId = next->Id();
    static_cast<CallbackActor*>(next)->draw = [&](const ActorTransform&)
    {
        graph.Destroy(nextId);
    };
    graph.Render();
    assert(graph.Size() == 0);
    auto& throwing = graph.GetOrCreate<CallbackActor>({5}, 0);
    auto throwingId = throwing.Id();
    throwing.update = [&](float)
    {
        graph.Destroy(throwingId);
        throw std::runtime_error("intentional");
    };
    try
    {
        graph.Update(.01f);
    }
    catch (const std::runtime_error&)
    {
    }
    assert(graph.Size() == 0);
    graph.Update(.01f);
}

void CheckCombatLifecycle()
{
    SceneGraph graph;
    LevelOne::Session level;
    LevelOne::Start(level, 123);
    auto generation = level.generation;
    auto& simulation = graph.GetOrCreate<CombatSimulationActor>({1}, 0, level);
    simulation.movement = {1, 1};
    auto before = level.elapsed;
    graph.Update(.01f);
    assert(level.elapsed > before);
    simulation.enabled = false;
    before = level.elapsed;
    graph.Update(.01f);
    assert(level.elapsed == before);
    level.drops.front().actorId = ++level.nextActorId;
    LevelOne::Start(level, 123);
    assert(level.generation == generation + 1 && level.nextActorId == 0);
    assert(level.drops.front().actorId == 0);
}

int main()
{
    CheckTransformsAndParents();
    CheckLayersVisibilityAndUpdates();
    CheckReuseAndStreaming();
    CheckDeferredMutation();
    CheckCombatLifecycle();
}
