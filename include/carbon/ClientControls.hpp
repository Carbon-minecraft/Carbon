#pragma once

namespace carbon {

// MultiPlayerGameMode.continueDestroyBlock returns early while its creative
// delay is positive, even on the tick it decrements to zero.
class CreativeBreakDelay {
public:
    bool beginFreshPress() noexcept { remaining_ = 5; return true; }
    bool beginHeldTick() noexcept {
        if (remaining_ > 0) { --remaining_; return false; }
        remaining_ = 5;
        return true;
    }
    int remaining() const noexcept { return remaining_; }
private:
    int remaining_{};
};

} // namespace carbon
