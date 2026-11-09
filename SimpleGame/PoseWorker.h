#pragma once
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <cstdint>

// One CPU worker. Actor requests coalesce, while equal geometry keys share one build.
// The player uses the priority lane so background NPC poses cannot freeze its gait.
template<class Value> class PoseWorker
{
public:
    struct Result { std::uint64_t family = 0, key = 0, serial = 0; Value value; };
    PoseWorker() : thread([this] { Run(); }) {}
    ~PoseWorker()
    {
        { std::lock_guard<std::mutex> lock(mutex); stopped = true; }
        ready.notify_all(); thread.join();
    }
    void Submit(std::uint64_t family, std::uint64_t key, std::function<Value()> build,
                bool priority = false, std::uint64_t serial = 0)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopped) return;
        auto old = pending.find(family);
        if (old != pending.end() && old->second != key) RemoveFamily(family);
        if (busy && activeKey == key)
        {
            RemoveFamily(family);
            activeFamilies[family] = serial;
            return;
        }
        auto found = jobs.find(key);
        if (found != jobs.end())
        {
            found->second.families[family] = serial;
            pending[family] = key;
            if (priority && !found->second.priority)
            {
                found->second.priority = true;
                queue.erase(std::remove(queue.begin(), queue.end(), key), queue.end());
                queue.push_front(key);
            }
            return;
        }
        if (pending.size() >= 128)
        {
            if (!priority) return;
            // A newly visible player must not lose its request to a full NPC queue.
            auto victim = std::find_if(queue.rbegin(), queue.rend(), [&](std::uint64_t k) { return !jobs.at(k).priority; });
            if (victim == queue.rend()) return;
            const auto removeKey = *victim;
            for (const auto& item : jobs.at(removeKey).families) pending.erase(item.first);
            jobs.erase(removeKey);
            queue.erase(std::remove(queue.begin(), queue.end(), removeKey), queue.end());
        }
        Job job;
        job.build = std::move(build);
        job.priority = priority;
        job.families[family] = serial;
        jobs.emplace(key, std::move(job));
        pending[family] = key;
        if (priority) queue.push_front(key); else queue.push_back(key);
        ready.notify_one();
    }
    bool Poll(Result& result)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (completed.empty()) return false;
        auto& group = completed.front();
        auto subscriber = group.families.begin();
        result.family = subscriber->first;
        result.serial = subscriber->second;
        result.key = group.key;
        group.families.erase(subscriber);
        if (group.families.empty())
        {
            result.value = std::move(group.value);
            completed.pop_front();
        }
        else result.value = group.value;
        ready.notify_one();
        return true;
    }
private:
    struct Job
    {
        std::function<Value()> build;
        std::map<std::uint64_t, std::uint64_t> families;
        bool priority = false;
    };
    struct Completion
    {
        std::uint64_t key;
        Value value;
        std::map<std::uint64_t, std::uint64_t> families;
    };
    std::mutex mutex;
    std::condition_variable ready;
    bool stopped = false, busy = false;
    std::uint64_t activeKey = 0;
    std::map<std::uint64_t, std::uint64_t> activeFamilies;
    std::map<std::uint64_t, Job> jobs;
    std::map<std::uint64_t, std::uint64_t> pending;
    std::deque<std::uint64_t> queue;
    std::deque<Completion> completed;
    // Last member: synchronization state exists before the thread starts.
    std::thread thread;

    void RemoveFamily(std::uint64_t family)
    {
        auto old = pending.find(family);
        if (old == pending.end()) return;
        const auto key = old->second;
        auto job = jobs.find(key);
        job->second.families.erase(family);
        pending.erase(old);
        if (job->second.families.empty())
        {
            jobs.erase(job);
            queue.erase(std::remove(queue.begin(), queue.end(), key), queue.end());
        }
    }

    void Run()
    {
        for (;;)
        {
            Job job;
            std::uint64_t key;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock, [this] { return stopped || (!queue.empty() && completed.size() < 8); });
                if (stopped) return;
                key = queue.front(); queue.pop_front();
                job = std::move(jobs.at(key)); jobs.erase(key);
                for (const auto& item : job.families) pending.erase(item.first);
                busy = true; activeKey = key; activeFamilies = std::move(job.families);
            }
            Value value = job.build(); // Caller converts exceptions into result values.
            {
                std::lock_guard<std::mutex> lock(mutex);
                busy = false;
                if (stopped) return;
                completed.push_back(Completion{key, std::move(value), std::move(activeFamilies)});
            }
        }
    }
};
