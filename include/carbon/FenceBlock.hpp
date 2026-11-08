#pragma once

// Dry oak-fence subset of Java 26.3 FenceBlock/CrossCollisionBlock.
// WATERLOGGED, fence gates, exception blocks and non-oak fence tags are outside
// this world slice. Connection support still follows source FULL face-sturdy
// checks for every supported neighboring block shape.
#include "carbon/PlayableWorld.hpp"
#include "carbon/StateProperties.hpp"
#include "carbon/VoxelShapes.hpp"

namespace carbon {

inline const auto& fenceNorthProperty() {
    static const auto property = BooleanProperty::create("north");
    return property;
}
inline const auto& fenceEastProperty() {
    static const auto property = BooleanProperty::create("east");
    return property;
}
inline const auto& fenceWestProperty() {
    static const auto property = BooleanProperty::create("west");
    return property;
}
inline const auto& fenceSouthProperty() {
    static const auto property = BooleanProperty::create("south");
    return property;
}

inline const StateDefinition<BlockId>& oakFenceStateDefinition() {
    // FenceBlock.createBlockStateDefinition source order is NORTH, EAST, WEST,
    // SOUTH. StateDefinition preserves property membership while canonical
    // serialization remains name-sorted, as StateDefinition does in Java.
    static const auto definition = StateDefinition<BlockId>::Builder(BlockId::OakFence)
        .add(fenceNorthProperty(), fenceEastProperty(), fenceWestProperty(), fenceSouthProperty()).create();
    return definition;
}

inline VoxelShape fenceOutlineShape(FenceConnections c) {
    auto shape = Shapes::box(0.375, 0.0, 0.375, 0.625, 1.0, 0.625);
    constexpr double lo = 0.375, hi = 0.625;
    if (c.north) shape = Shapes::join(shape, Shapes::box(lo, 0.375, 0.0, hi, 0.9375, 0.5), BooleanOp::Or);
    if (c.east)  shape = Shapes::join(shape, Shapes::box(0.5, 0.375, lo, 1.0, 0.9375, hi), BooleanOp::Or);
    if (c.south) shape = Shapes::join(shape, Shapes::box(lo, 0.375, 0.5, hi, 0.9375, 1.0), BooleanOp::Or);
    if (c.west)  shape = Shapes::join(shape, Shapes::box(0.0, 0.375, lo, 0.5, 0.9375, hi), BooleanOp::Or);
    return shape;
}

inline VoxelShape fenceCollisionShape(FenceConnections c) {
    auto shape = Shapes::box(0.375, 0.0, 0.375, 0.625, 1.5, 0.625);
    constexpr double lo = 0.375, hi = 0.625;
    if (c.north) shape = Shapes::join(shape, Shapes::box(lo, 0.0, 0.0, hi, 1.5, 0.5), BooleanOp::Or);
    if (c.east)  shape = Shapes::join(shape, Shapes::box(0.5, 0.0, lo, 1.0, 1.5, hi), BooleanOp::Or);
    if (c.south) shape = Shapes::join(shape, Shapes::box(lo, 0.0, 0.5, hi, 1.5, 1.0), BooleanOp::Or);
    if (c.west)  shape = Shapes::join(shape, Shapes::box(0.0, 0.0, lo, 0.5, 1.5, hi), BooleanOp::Or);
    return shape;
}

} // namespace carbon
