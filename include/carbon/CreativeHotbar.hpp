#pragma once

// The supported client-side portion of Inventory and pick-block behavior.
// This is nine block-item identities, not a general ItemStack inventory.
#include "carbon/PlayableWorld.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace carbon {

class CreativeHotbar {
public:
    static constexpr std::size_t SLOT_COUNT = 9;

    // Only entries backed by this native block/item subset are populated.
    // Air represents an empty slot; it is not an item.
    std::array<BlockId, SLOT_COUNT> slots{
        BlockId::GrassBlock, BlockId::Dirt, BlockId::Stone,
        BlockId::Bedrock, BlockId::Air, BlockId::StoneSlab,
        BlockId::StoneStairs, BlockId::Air, BlockId::Air
    };
    int selectedSlot{};

    void selectSlot(int slot) {
        if (slot < 0 || slot >= static_cast<int>(SLOT_COUNT))
            throw std::invalid_argument("Invalid selected hotbar slot");
        selectedSlot = slot;
    }

    // Mirrors ScrollWheelHandler plus getNextScrollWheelSelection. Callers pass
    // scaled wheel units (Windows WM_MOUSEWHEEL delta / WHEEL_DELTA); fractional
    // events accumulate, and positive wheel input decrements the selected index.
    void scroll(double wheel) noexcept {
        if (accumulatedWheel_ != 0.0 &&
            ((wheel > 0.0) != (accumulatedWheel_ > 0.0) || wheel == 0.0)) accumulatedWheel_ = 0.0;
        accumulatedWheel_ += wheel;
        const int whole = static_cast<int>(accumulatedWheel_);
        if (whole == 0) return;
        accumulatedWheel_ -= whole;
        const int step = whole > 0 ? 1 : -1;
        selectedSlot = (selectedSlot - step + static_cast<int>(SLOT_COUNT)) % static_cast<int>(SLOT_COUNT);
    }

    // Mirrors ServerGamePacketListenerImpl.tryPickItem for supported block
    // items. With no backing main inventory, a full hotbar picks its selected
    // slot, matching Inventory.getSuitableHotbarSlot when no items are enchanted.
    bool pickBlock(BlockId block) noexcept {
        if (!isSupportedBlockItem(block)) return false;
        for (std::size_t i = 0; i < slots.size(); ++i) {
            if (slots[i] == block) {
                selectedSlot = static_cast<int>(i);
                return true;
            }
        }
        for (int offset = 0; offset < static_cast<int>(SLOT_COUNT); ++offset) {
            const int index = (selectedSlot + offset) % static_cast<int>(SLOT_COUNT);
            if (slots[static_cast<std::size_t>(index)] == BlockId::Air) {
                selectedSlot = index;
                slots[static_cast<std::size_t>(index)] = block;
                return true;
            }
        }
        slots[static_cast<std::size_t>(selectedSlot)] = block;
        return true;
    }

    static constexpr bool isSupportedBlockItem(BlockId block) noexcept {
        switch (block) {
        case BlockId::Bedrock:
        case BlockId::Dirt:
        case BlockId::GrassBlock:
        case BlockId::Stone:
        case BlockId::StoneSlab:
        case BlockId::StoneStairs:
        case BlockId::OakFence:
            return true;
        case BlockId::Air:
            return false;
        }
        return false;
    }

private:
    double accumulatedWheel_{};
};

} // namespace carbon
