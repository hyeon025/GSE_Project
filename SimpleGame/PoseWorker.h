#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <cstdint>

// One CPU worker. Queued requests for an appearance coalesce to its newest pose.
template<class Value> class PoseWorker
{
public:
    struct Result { std::uint64_t family = 0, key = 0; Value value; };
    PoseWorker() : thread([this] { Run(); }) {}
    ~PoseWorker()
    {
        { std::lock_guard<std::mutex> lock(mutex); stopped = true; }
        ready.notify_all(); thread.join();
    }
    void Submit(std::uint64_t family, std::uint64_t key, std::function<Value()> build)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if ((busy && activeFamily == family && activeKey == key) || stopped) return;
        if (!jobs.count(family)) {
            if (jobs.size() >= 128) return;
            queue.push_back(family);
        }
        jobs[family] = Job{key, std::move(build)}; ready.notify_one();
    }
    bool Poll(Result& result)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (completed.empty()) return false;
        result = std::move(completed.front()); completed.pop_front(); ready.notify_one(); return true;
    }
private:
    struct Job { std::uint64_t key; std::function<Value()> build; };
    std::mutex mutex;
    std::condition_variable ready;
    bool stopped = false, busy = false;
    std::uint64_t activeFamily = 0, activeKey = 0;
    std::map<std::uint64_t, Job> jobs;
    std::deque<std::uint64_t> queue;
    std::deque<Result> completed;
    // Last member: synchronization state exists before the thread starts.
    std::thread thread;
    void Run()
    {
        for (;;) {
            Job job; std::uint64_t family;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock, [this] { return stopped || (!queue.empty() && completed.size() < 8); });
                if (stopped) return;
                family = queue.front(); queue.pop_front(); job = std::move(jobs.at(family)); jobs.erase(family);
                busy = true; activeFamily = family; activeKey = job.key;
            }
            Value value = job.build(); // Caller converts exceptions into result values.
            {
                std::lock_guard<std::mutex> lock(mutex); busy = false;
                if (stopped) return;
                completed.push_back(Result{family, job.key, std::move(value)});
            }
        }
    }
};
