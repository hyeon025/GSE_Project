#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace Affinity
{
    enum Reason { Conversation, Bread, Rescue, Support, Threat, Attack, Trade, Training, ReasonCount };
    constexpr size_t HistoryLimit = 128;
    constexpr double TalkCooldown = 60, TradeCooldown = 30;

    struct Memory
    {
        double lastTalk = -1, lastTrade = -1, lastChange = -1;
        int lastDelta = 0, lastReason = -1;
    };

    struct Event
    {
        uint64_t id = 0;
        int npcId = 0, reason = 0, before = 0, after = 0;
        double time = 0;
    };

    struct State
    {
        uint64_t nextId = 1, revision = 0;
        std::vector<Event> events;
    };

    inline const wchar_t* Tier(int score)
    {
        if (score >= 75) return L"\uae4a\uc740 \uc720\ub300";
        if (score >= 50) return L"\uce5c\ubc00";
        if (score >= 25) return L"\uc6b0\ud638";
        if (score >= 0) return L"\uc911\ub9bd";
        if (score > -50) return L"\uacbd\uacc4";
        return L"\uc801\ub300";
    }

    inline const wchar_t* ReasonName(int reason)
    {
        static const wchar_t* labels[] = {L"\ub300\ud654", L"\ube75 \ub098\ub204\uae30", L"\uce58\ub8cc", L"\uc0b6\uc758 \ubcc0\ud654 \uc9c0\uc6d0",
                                         L"\uc704\ud611", L"\uacf5\uaca9", L"\uac70\ub798", L"\ud568\uaed8 \uc218\ub828"};
        return reason >= 0 && reason < ReasonCount ? labels[reason] : L"\uae30\ub85d \uc5c6\uc74c";
    }

    inline int Amount(Reason reason)
    {
        static const int values[] = {5, 25, 35, 15, -40, -15, 2, 5};
        return reason >= 0 && reason < ReasonCount ? values[reason] : 0;
    }

    inline double Remaining(double now, double last, double cooldown)
    {
        return last < 0 ? 0 : std::max(0.0, last + cooldown - now);
    }

    inline std::wstring Signed(int value)
    {
        return (value > 0 ? L"+" : L"") + std::to_wstring(value);
    }

    // All relationship writes go through this function. Call only after the action succeeds.
    // Cooldowns use saved simulation time, so reopening dialogue/reloading cannot farm rewards.
    template<class World, class Npc>
    int Apply(World& w, Npc& n, Reason reason)
    {
        if (!n.life.alive || n.id <= 0 || reason < 0 || reason >= ReasonCount) return 0;
        double now = w.society.elapsed;
        if (reason == Conversation && Remaining(now, n.affinity.lastTalk, TalkCooldown) > 1e-8) return 0;
        if (reason == Trade && Remaining(now, n.affinity.lastTrade, TradeCooldown) > 1e-8) return 0;
        if (reason == Conversation) n.affinity.lastTalk = now;
        if (reason == Trade) n.affinity.lastTrade = now;
        const int before = n.relationship;
        n.relationship = std::max(-100, std::min(100, before + Amount(reason)));
        n.trust = std::min(3, std::max(0, n.relationship / 25));
        w.dirty = true;
        if (before == n.relationship) return 0;
        Event e;
        e.id = w.affinity.nextId++;
        e.npcId = n.id;
        e.reason = reason;
        e.before = before;
        e.after = n.relationship;
        e.time = now;
        auto& events = w.affinity.events;
        if (events.size() >= HistoryLimit) events.erase(events.begin());
        events.push_back(e);
        ++w.affinity.revision;
        n.affinity.lastChange = now;
        n.affinity.lastDelta = e.after - e.before;
        n.affinity.lastReason = reason;
        return n.affinity.lastDelta;
    }

    // Store indices, never pointers into the growing NPC vector. Equal scores use stable IDs.
    template<class World>
    std::vector<size_t> Ranking(const World& w, int village = -1, bool metOnly = false, bool descending = true)
    {
        std::vector<size_t> result;
        for (size_t i = 0; i < w.npcs.size(); ++i)
        {
            const auto& n = w.npcs[i];
            if (n.life.alive && (village < 0 || n.life.village == village) && (!metOnly || n.met))
                result.push_back(i);
        }
        std::sort(result.begin(), result.end(), [&](size_t a, size_t b)
        {
            const auto& x = w.npcs[a];
            const auto& y = w.npcs[b];
            if (x.relationship != y.relationship)
                return descending ? x.relationship > y.relationship : x.relationship < y.relationship;
            return x.id < y.id;
        });
        return result;
    }

    template<class World>
    void Write(std::ostream& out, const World& w)
    {
        out << "AFFINITY 1 " << w.npcs.size() << ' ' << w.affinity.nextId << ' ' << w.affinity.events.size() << '\n';
        for (const auto& n : w.npcs)
        {
            const auto& m = n.affinity;
            out << n.id << ' ' << m.lastTalk << ' ' << m.lastTrade << ' ' << m.lastChange << ' '
                << m.lastDelta << ' ' << m.lastReason << '\n';
        }
        for (const auto& e : w.affinity.events)
            out << e.id << ' ' << e.npcId << ' ' << e.time << ' ' << e.reason << ' ' << e.before << ' ' << e.after << '\n';
    }

    template<class World>
    bool Read(std::istream& in, World& w)
    {
        std::string magic;
        int version = 0;
        size_t count = 0, eventCount = 0;
        State state;
        if (!(in >> magic >> version >> count >> state.nextId >> eventCount) || magic != "AFFINITY" || version != 1 ||
            count != w.npcs.size() || eventCount > HistoryLimit || !state.nextId || state.nextId > 9007199254740991ULL)
            return false;
        auto validTime = [&](double t) { return std::isfinite(t) && (t == -1 || (t >= 0 && t <= w.society.elapsed + 1e-6)); };
        std::vector<Memory> memories(count);
        for (size_t i = 0; i < count; ++i)
        {
            int id = 0;
            auto& m = memories[i];
            if (!(in >> id >> m.lastTalk >> m.lastTrade >> m.lastChange >> m.lastDelta >> m.lastReason) ||
                id != w.npcs[i].id || !validTime(m.lastTalk) || !validTime(m.lastTrade) || !validTime(m.lastChange) ||
                m.lastDelta < -40 || m.lastDelta > 35 || m.lastReason < -1 || m.lastReason >= ReasonCount ||
                (m.lastChange < 0 ? (m.lastReason != -1 || m.lastDelta != 0) : (m.lastReason < 0 || m.lastDelta == 0)))
                return false;
        }
        for (size_t i = 0; i < eventCount; ++i)
        {
            Event e;
            if (!(in >> e.id >> e.npcId >> e.time >> e.reason >> e.before >> e.after) ||
                !e.id || e.id >= state.nextId || e.npcId < 1 || (size_t)e.npcId > count ||
                !validTime(e.time) || e.time < 0 || e.reason < 0 || e.reason >= ReasonCount ||
                e.before < -100 || e.before > 100 || e.after < -100 || e.after > 100 || e.before == e.after ||
                e.after != std::max(-100, std::min(100, e.before + Amount((Reason)e.reason))) ||
                (!state.events.empty() && (e.id <= state.events.back().id || e.time < state.events.back().time)))
                return false;
            state.events.push_back(e);
        }
        w.affinity = std::move(state);
        for (size_t i = 0; i < count; ++i) w.npcs[i].affinity = memories[i];
        return true;
    }
}
