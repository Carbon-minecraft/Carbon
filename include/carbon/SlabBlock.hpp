#pragma once

// Dry stone-slab subset of Java 26.3 SlabBlock. Fluid state and waterlogging
// are intentionally unsupported; this definition models only TYPE.
#include "carbon/PlayableWorld.hpp"
#include "carbon/StateProperties.hpp"
#include "carbon/VoxelShapes.hpp"

namespace carbon {

inline const auto& slabTypeProperty() {
    static const auto property = EnumProperty<SlabType>::create(
        "type", {SlabType::Top, SlabType::Bottom, SlabType::Double},
        [](SlabType type) -> std::string {
            switch(type) {
            case SlabType::Top: return "top";
            case SlabType::Bottom: return "bottom";
            case SlabType::Double: return "double";
            }
            return "bottom";
        });
    return property;
}

inline const StateDefinition<BlockId>& stoneSlabStateDefinition() {
    static const auto definition = StateDefinition<BlockId>::Builder(BlockId::StoneSlab)
        .add(slabTypeProperty()).create();
    return definition;
}

inline VoxelShape slabVoxelShape(SlabType type) {
    switch(type) {
    case SlabType::Bottom: return Shapes::box(0,0,0,1,0.5,1);
    case SlabType::Top: return Shapes::box(0,0.5,0,1,1,1);
    case SlabType::Double: return Shapes::block();
    }
    return Shapes::box(0,0,0,1,0.5,1);
}

// Direct SlabBlock.getStateForPlacement height/face rule. Waterlogged remains
// absent because this gameplay slice has no fluids.
inline SlabType slabTypeForPlacement(Direction clickedFace, double hitY, int blockY) {
    return clickedFace != Direction::Down &&
           (clickedFace == Direction::Up || !(hitY - blockY > 0.5))
        ? SlabType::Bottom : SlabType::Top;
}

inline bool slabCanReplace(SlabType current, Direction clickedFace, double hitY,
                           int blockY, bool clickedOnBlock) {
    if(current == SlabType::Double) return false;
    if(!clickedOnBlock) return true;
    const bool above = hitY - blockY > 0.5;
    const bool horizontal = axis(clickedFace) != Axis::Y;
    return current == SlabType::Bottom
        ? clickedFace == Direction::Up || (above && horizontal)
        : clickedFace == Direction::Down || (!above && horizontal);
}

} // namespace carbon
