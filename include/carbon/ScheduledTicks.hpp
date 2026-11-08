#pragma once

// A source-faithful data/queue subset of Java 26.3's TickPriority,
// ScheduledTick, SavedTick, LevelChunkTicks and BlackholeTickAccess.
// Tick types are represented by opaque identity tokens: Java uniqueness uses
// type reference identity, so callers should pass a stable object address or ID.
// Codec/registry integration and the world-wide LevelTicks simulation are out
// of scope.

#include "carbon/Coordinates.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

namespace carbon {

enum class TickPriority : std::uint8_t {
    ExtremelyHigh = 0, VeryHigh, High, Normal, Low, VeryLow, ExtremelyLow
};

constexpr std::int32_t tickPriorityValue(TickPriority p) {
    return static_cast<std::int32_t>(p) - 3;
}
constexpr std::int32_t tickPriorityOrdinal(TickPriority p) {
    return static_cast<std::int32_t>(p);
}
constexpr TickPriority tickPriorityByValue(std::int32_t value) {
    return value < -3 ? TickPriority::ExtremelyHigh
         : value > 3 ? TickPriority::ExtremelyLow
         : static_cast<TickPriority>(value + 3);
}

using TickTypeIdentity = std::uintptr_t;

struct SavedTick {
    TickTypeIdentity type{};
    BlockPos pos{};
    std::int32_t delay{};
    TickPriority priority{TickPriority::Normal};

    static SavedTick probe(TickTypeIdentity type, BlockPos pos) {
        return {type, pos, 0, TickPriority::Normal};
    }
};

struct ScheduledTick {
    TickTypeIdentity type{};
    BlockPos pos{};
    std::int64_t triggerTick{};
    TickPriority priority{TickPriority::Normal};
    std::int64_t subTickOrder{};

    static ScheduledTick probe(TickTypeIdentity type, BlockPos pos) {
        return {type, pos, 0, TickPriority::Normal, 0};
    }
    SavedTick toSavedTick(std::int64_t currentTick) const {
        // Java narrows the long subtraction to int, retaining the low 32 bits.
        const auto delta = std::bit_cast<std::uint64_t>(triggerTick)
                         - std::bit_cast<std::uint64_t>(currentTick);
        return {type, pos, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(delta)), priority};
    }
    static ScheduledTick fromSavedTick(const SavedTick& saved, std::int64_t currentTick,
                                       std::int64_t subOrder) {
        const auto trigger = std::bit_cast<std::uint64_t>(currentTick)
                           + static_cast<std::uint64_t>(static_cast<std::int64_t>(saved.delay));
        return {saved.type, saved.pos, std::bit_cast<std::int64_t>(trigger), saved.priority, subOrder};
    }
};

inline bool drainOrderBefore(const ScheduledTick& a, const ScheduledTick& b) {
    if (a.triggerTick != b.triggerTick) return a.triggerTick < b.triggerTick;
    if (a.priority != b.priority) return tickPriorityOrdinal(a.priority) < tickPriorityOrdinal(b.priority);
    return a.subTickOrder < b.subTickOrder;
}
inline bool intraTickDrainOrderBefore(const ScheduledTick& a, const ScheduledTick& b) {
    if (a.priority != b.priority) return tickPriorityOrdinal(a.priority) < tickPriorityOrdinal(b.priority);
    return a.subTickOrder < b.subTickOrder;
}

namespace tick_detail {
struct IdentityPos {
    TickTypeIdentity type{};
    BlockPos pos{};
    bool operator==(const IdentityPos& other) const {
        return type == other.type && pos == other.pos;
    }
};
struct IdentityPosHash {
    std::size_t operator()(const IdentityPos& key) const noexcept {
        const auto p = static_cast<const Vec3i&>(key.pos);
        const auto h = static_cast<std::uint32_t>(p.hashCode());
        return (static_cast<std::size_t>(key.type) * 31U) ^ h;
    }
};
inline std::int64_t wrapAdd(std::int64_t a, std::int64_t b) {
    return std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(a) + std::bit_cast<std::uint64_t>(b));
}
}

class LevelChunkTicks {
public:
    using TickAdded = std::function<void(LevelChunkTicks&, const ScheduledTick&)>;
    using Predicate = std::function<bool(const ScheduledTick&)>;

    LevelChunkTicks() = default;
    explicit LevelChunkTicks(std::vector<SavedTick> pending) : pending_(std::move(pending)) {
        for (const auto& tick : *pending_) unique_.insert({tick.type, tick.pos});
    }

    void setOnTickAdded(TickAdded callback) { onTickAdded_ = std::move(callback); }
    const ScheduledTick* peek() const { return queue_.empty() ? nullptr : &queue_.top(); }
    std::optional<ScheduledTick> poll() {
        if (queue_.empty()) return std::nullopt;
        auto result = queue_.top();
        queue_.pop();
        unique_.erase({result.type, result.pos});
        return result;
    }
    void schedule(const ScheduledTick& tick) {
        if (!unique_.insert({tick.type, tick.pos}).second) return;
        scheduleUnchecked(tick);
    }
    bool hasScheduledTick(BlockPos pos, TickTypeIdentity type) const {
        return unique_.contains({type, pos});
    }
    void removeIf(const Predicate& test) {
        std::vector<ScheduledTick> kept;
        while (!queue_.empty()) {
            const auto tick = queue_.top(); queue_.pop();
            if (test(tick)) unique_.erase({tick.type, tick.pos});
            else kept.push_back(tick);
        }
        for (const auto& tick : kept) queue_.push(tick);
    }
    std::vector<ScheduledTick> getAll() const {
        auto copy = queue_;
        std::vector<ScheduledTick> result;
        result.reserve(copy.size());
        while (!copy.empty()) { result.push_back(copy.top()); copy.pop(); }
        return result;
    }
    std::size_t count() const { return queue_.size() + (pending_ ? pending_->size() : 0); }
    std::vector<SavedTick> pack(std::int64_t currentTick) const {
        std::vector<SavedTick> result;
        result.reserve(count());
        if (pending_) result.insert(result.end(), pending_->begin(), pending_->end());
        auto ticks = getAll();
        std::sort(ticks.begin(), ticks.end(), [](const auto& a, const auto& b) {
            return a.subTickOrder < b.subTickOrder;
        });
        for (const auto& tick : ticks) result.push_back(tick.toSavedTick(currentTick));
        return result;
    }
    void unpack(std::int64_t currentTick) {
        if (!pending_) return;
        // Java starts at -pending.size() as an int and increments with int wrap.
        auto order = std::bit_cast<std::int32_t>(0U - static_cast<std::uint32_t>(pending_->size()));
        for (const auto& saved : *pending_) {
            scheduleUnchecked(ScheduledTick::fromSavedTick(saved, currentTick, order));
            order = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(order) + 1U);
        }
        pending_.reset();
    }

private:
    struct Later { bool operator()(const ScheduledTick& a, const ScheduledTick& b) const { return drainOrderBefore(b, a); } };
    using Queue = std::priority_queue<ScheduledTick, std::vector<ScheduledTick>, Later>;
    void scheduleUnchecked(const ScheduledTick& tick) {
        queue_.push(tick);
        if (onTickAdded_) onTickAdded_(*this, tick);
    }
    Queue queue_;
    std::optional<std::vector<SavedTick>> pending_;
    std::unordered_set<tick_detail::IdentityPos, tick_detail::IdentityPosHash> unique_;
    TickAdded onTickAdded_;
};

class BlackholeTickAccess {
public:
    static void schedule(const ScheduledTick&) {}
    static bool hasScheduledTick(BlockPos, TickTypeIdentity) { return false; }
    static constexpr std::size_t count() { return 0; }
};

}
