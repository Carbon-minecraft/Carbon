#include "carbon/ClientControls.hpp"

#include <cassert>
#include <iostream>

int main() {
    carbon::CreativeBreakDelay delay;
    assert(delay.beginFreshPress()); // first creative attack is immediate
    for (int tick=0; tick<5; ++tick) assert(!delay.beginHeldTick());
    assert(delay.remaining() == 0);
    assert(delay.beginHeldTick()); // Java continues on the tick after the delay reaches zero
    assert(delay.remaining() == 5);
    assert(!delay.beginHeldTick());
    // A fresh attack bypasses the held continuation delay, even after release.
    assert(delay.beginFreshPress());
    assert(delay.remaining() == 5);
    std::cout << "creative fresh press and held-break delay match source cadence\n";
}
