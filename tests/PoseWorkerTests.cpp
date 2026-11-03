#include "../SimpleGame/PoseWorker.h"
#include <cassert>
#include <future>
#include <chrono>

int main()
{
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
