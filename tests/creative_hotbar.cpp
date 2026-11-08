#include "carbon/CreativeHotbar.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    using carbon::BlockId;
    carbon::CreativeHotbar hotbar;
    assert(carbon::CreativeHotbar::SLOT_COUNT == 9);
    assert(hotbar.selectedSlot == 0);
    assert(hotbar.slots[4] == BlockId::Air && hotbar.slots[7] == BlockId::Air);

    hotbar.selectSlot(8);
    hotbar.scroll(0.5); // fractional MouseHandler scroll is buffered
    assert(hotbar.selectedSlot == 8);
    hotbar.scroll(0.5); // Java ScrollWheelHandler subtracts sign(wheel).
    assert(hotbar.selectedSlot == 7);
    hotbar.scroll(-0.5);
    hotbar.scroll(0.25); // reversal clears the old partial delta
    assert(hotbar.selectedSlot == 7);
    hotbar.scroll(0.75);
    assert(hotbar.selectedSlot == 6);
    hotbar.scroll(0.0);
    assert(hotbar.selectedSlot == 6);
    hotbar.scroll(1.0);
    assert(hotbar.selectedSlot == 5);
    hotbar.selectSlot(0);
    hotbar.scroll(1.0);
    assert(hotbar.selectedSlot == 8);
    hotbar.selectSlot(4);
    hotbar.scroll(0.25);
    hotbar.scroll(0.0); // source handler clears a partial delta on sign reversal to zero
    hotbar.scroll(0.75);
    assert(hotbar.selectedSlot == 4);
    hotbar.scroll(0.25);
    assert(hotbar.selectedSlot == 3);

    bool rejected = false;
    try { hotbar.selectSlot(9); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    // Vanilla selects a matching item instead of inserting a duplicate.
    assert(hotbar.pickBlock(BlockId::StoneSlab));
    assert(hotbar.selectedSlot == 5);
    assert(hotbar.slots[5] == BlockId::StoneSlab);
    assert(hotbar.pickBlock(BlockId::StoneStairs));
    assert(hotbar.selectedSlot == 6);

    // Supported block items go into the first empty slot starting at the
    // current selection; empty/missing items do not fabricate a stack.
    hotbar.selectSlot(7);
    assert(hotbar.pickBlock(BlockId::Air) == false);
    assert(hotbar.slots[7] == BlockId::Air);
    assert(hotbar.pickBlock(BlockId::StoneSlab) && hotbar.selectedSlot == 5);
    assert(hotbar.pickBlock(BlockId::OakFence));
    assert(hotbar.selectedSlot == 7 && hotbar.slots[7] == BlockId::OakFence);
    hotbar.slots[5] = BlockId::Air;
    hotbar.selectSlot(8);
    assert(hotbar.pickBlock(BlockId::StoneSlab));
    assert(hotbar.selectedSlot == 8 && hotbar.slots[8] == BlockId::StoneSlab);

    std::cout << "nine-slot selection, wheel wrap and supported creative pick-block rules passed\n";
}
