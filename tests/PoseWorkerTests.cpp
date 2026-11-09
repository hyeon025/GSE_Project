#include "../SimpleGame/PoseWorker.h"
#include <cassert>
#include <future>
#include <chrono>
#include <atomic>

template<class T> typename PoseWorker<T>::Result Next(PoseWorker<T>& worker)
{
    typename PoseWorker<T>::Result result;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!worker.Poll(result))
    {
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
    return result;
}

void SharedBuilds()
{
    PoseWorker<int> worker;
    std::promise<void> started, release;
    auto gate = release.get_future().share();
    std::atomic<int> builds(0);
    worker.Submit(1, 10, [&] { ++builds; started.set_value(); gate.wait(); return 10; }, false, 101);
    assert(started.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    worker.Submit(4, 10, [] { assert(false); return -1; }, false, 104); // Join an in-flight shared pose.
    worker.Submit(2, 20, [&] { ++builds; return 20; }, false, 202);
    worker.Submit(3, 20, [] { assert(false); return -1; }, false, 203); // Join a queued shared pose.
    release.set_value();
    auto a = Next(worker), b = Next(worker), c = Next(worker), d = Next(worker);
    assert(a.family == 1 && a.key == 10 && a.value == 10 && a.serial == 101);
    assert(b.family == 4 && b.key == 10 && b.value == 10 && b.serial == 104);
    assert(c.family == 2 && c.key == 20 && c.value == 20 && c.serial == 202);
    assert(d.family == 3 && d.key == 20 && d.value == 20 && d.serial == 203);
    assert(builds == 2); // Identity-specific completion history must not duplicate shared geometry work.
}

void PlayerPriority()
{
    PoseWorker<int> worker;
    std::promise<void> started, release;
    auto gate = release.get_future().share();
    worker.Submit(500, 500, [&] { started.set_value(); gate.wait(); return 500; });
    assert(started.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    for (unsigned family = 2; family < 130; ++family)
        worker.Submit(family, family, [family] { return (int)family; });
    worker.Submit(1, 999, [] { return 999; }, true, 1001); // Must survive a full NPC queue.
    release.set_value();
    auto active = Next(worker), player = Next(worker);
    assert(active.family == 500);
    assert(player.family == 1 && player.key == 999 && player.value == 999 && player.serial == 1001);
}

int main()
{
    SharedBuilds();
    PlayerPriority();
    PoseWorker<int> worker;
    std::promise<void> started, release;
    auto gate = release.get_future().share();
    worker.Submit(1, 10, [&] { started.set_value(); gate.wait(); return 10; });
    assert(started.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    worker.Submit(1, 10, [] { assert(false); return -1; }); // duplicate running pose is ignored
    worker.Submit(2, 20, [] { return 20; });
    worker.Submit(2, 21, [] { return 21; }); // only newest pending pose survives
    release.set_value();
    PoseWorker<int>::Result result;
    int count = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (count < 2 && std::chrono::steady_clock::now() < deadline)
    {
        if (!worker.Poll(result)) { std::this_thread::yield(); continue; }
        if (count == 0) assert(result.family == 1 && result.key == 10 && result.value == 10);
        else assert(result.family == 2 && result.key == 21 && result.value == 21);
        ++count;
    }
    assert(count == 2);
    // Destruction wakes a producer waiting on the bounded completion queue.
    {
        PoseWorker<int> bounded;
        for (unsigned i = 0; i < 100; ++i) bounded.Submit(i, i, [i] { return (int)i; });
    }
}
