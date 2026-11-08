#include "carbon/ScheduledTicks.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

int main() {
    using namespace carbon;
    for (int id = -5; id <= 5; ++id) {
        const auto p = tickPriorityByValue(id);
        assert(tickPriorityValue(p) == (id < -3 ? -3 : id > 3 ? 3 : id));
        std::cout << tickPriorityOrdinal(p) << ' ';
    }
    std::cout << '\n';

    constexpr TickTypeIdentity A = 11, B = 12;
    const BlockPos p0{1, 2, 3}, p1{1, 2, 4};
    LevelChunkTicks queue;
    int callbacks = 0;
    queue.setOnTickAdded([&](LevelChunkTicks&, const ScheduledTick&) { ++callbacks; });
    queue.schedule({A, p0, 7, TickPriority::Low, 1});
    queue.schedule({A, p0, 2, TickPriority::ExtremelyHigh, 0}); // duplicate identity+position
    queue.schedule({B, p0, 7, TickPriority::Normal, -4});       // distinct type identity
    queue.schedule({A, p1, 7, TickPriority::VeryHigh, 4});      // distinct position
    assert(callbacks == 3 && queue.count() == 3);
    assert(queue.hasScheduledTick(p0, A) && !queue.hasScheduledTick(p1, B));
    while (auto tick = queue.poll())
        std::cout << tick->type << ':' << tick->pos.x << ',' << tick->pos.y << ',' << tick->pos.z
                  << ':' << tick->triggerTick << ':' << tickPriorityOrdinal(tick->priority)
                  << ':' << tick->subTickOrder << ' ';
    std::cout << "\n";
    assert(!queue.hasScheduledTick(p0, A));

    LevelChunkTicks pending({
        {A, p0, 3, TickPriority::Normal}, {A, p0, -8, TickPriority::Low},
        {B, p1, 0, TickPriority::ExtremelyLow}
    });
    assert(pending.count() == 3 && pending.hasScheduledTick(p0, A));
    pending.setOnTickAdded([&](LevelChunkTicks&, const ScheduledTick&) { ++callbacks; });
    pending.schedule({A, p0, 8, TickPriority::High, 9}); // pending duplicates block new schedule
    assert(callbacks == 3);
    pending.unpack(-2);
    assert(callbacks == 6 && pending.count() == 3);
    auto unpacked = pending.getAll();
    std::sort(unpacked.begin(), unpacked.end(), drainOrderBefore);
    for (const auto& tick : unpacked)
        std::cout << tick.type << ':' << tick.triggerTick << ':' << tickPriorityOrdinal(tick.priority)
                  << ':' << tick.subTickOrder << ' ';
    std::cout << "\n";
    const auto packed = pending.pack(-2);
    assert(packed.size() == 3 && packed[0].delay == 3 && packed[1].delay == -8 && packed[2].delay == 0);

    LevelChunkTicks extreme;
    extreme.schedule({A, p0, std::numeric_limits<std::int64_t>::max(), TickPriority::Normal,
                      std::numeric_limits<std::int64_t>::max()});
    extreme.schedule({B, p0, std::numeric_limits<std::int64_t>::min(), TickPriority::Normal,
                      std::numeric_limits<std::int64_t>::min()});
    const auto first = extreme.poll();
    assert(first && first->triggerTick == std::numeric_limits<std::int64_t>::min());
    auto wrapped = ScheduledTick{A, p0, std::numeric_limits<std::int64_t>::min(), TickPriority::Normal, 0}
                       .toSavedTick(std::numeric_limits<std::int64_t>::max());
    assert(wrapped.delay == 1);
    std::cout << "EXTREME " << first->triggerTick << ' ' << wrapped.delay << '\n';

    LevelChunkTicks removals;
    removals.schedule({A, p0, 0, TickPriority::Normal, 0});
    removals.schedule({B, p0, 1, TickPriority::Normal, 1});
    removals.removeIf([](const ScheduledTick& t) { return t.type == A; });
    assert(removals.count() == 1 && !removals.hasScheduledTick(p0, A));
    assert(removals.hasScheduledTick(p0, B));
    assert(BlackholeTickAccess::count() == 0 && !BlackholeTickAccess::hasScheduledTick(p0, A));
    std::cout << "PASS callbacks=" << callbacks << " remove=1 blackhole=0\n";
}
