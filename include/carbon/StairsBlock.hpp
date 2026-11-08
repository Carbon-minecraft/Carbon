#pragma once

// Dry stone-stairs subset of Java 26.3 StairBlock. WATERLOGGED and all fluid
// scheduling/state are omitted because this gameplay slice has no fluids.
#include "carbon/PlayableWorld.hpp"
#include "carbon/StateProperties.hpp"
#include "carbon/VoxelShapes.hpp"

namespace carbon {

inline const auto& stairFacingProperty() {
    static const auto property = EnumProperty<Direction>::create(
        "facing", {Direction::Down, Direction::Up, Direction::North, Direction::South, Direction::West, Direction::East},
        {Direction::North, Direction::South, Direction::West, Direction::East},
        [](Direction value) { return std::string(name(value)); });
    return property;
}

inline const auto& stairHalfProperty() {
    static const auto property = EnumProperty<StairHalf>::create(
        "half", {StairHalf::Top, StairHalf::Bottom},
        [](StairHalf value) { return value == StairHalf::Top ? std::string("top") : std::string("bottom"); });
    return property;
}

inline const auto& stairsShapeProperty() {
    static const auto property = EnumProperty<StairsShape>::create(
        "shape", {StairsShape::Straight, StairsShape::InnerLeft, StairsShape::InnerRight,
                  StairsShape::OuterLeft, StairsShape::OuterRight},
        [](StairsShape value) -> std::string {
            switch (value) {
            case StairsShape::Straight: return "straight";
            case StairsShape::InnerLeft: return "inner_left";
            case StairsShape::InnerRight: return "inner_right";
            case StairsShape::OuterLeft: return "outer_left";
            case StairsShape::OuterRight: return "outer_right";
            }
            return "straight";
        });
    return property;
}

inline const StateDefinition<BlockId>& stoneStairsStateDefinition() {
    static const auto definition = StateDefinition<BlockId>::Builder(BlockId::StoneStairs)
        .add(stairFacingProperty(), stairHalfProperty(), stairsShapeProperty()).create();
    return definition;
}

// Java StairBlock's static SHAPE_OUTER / STRAIGHT / INNER unions. The source's
// four quarter-block riser cells rotate around the block center; lower/upper
// halves are mirrored vertically for TOP.
inline VoxelShape stairsVoxelShape(Direction facing, StairHalf half, StairsShape shape) {
    auto quarter = Shapes::box(0, 0.5, 0, 0.5, 1, 0.5);
    auto rotateCW = [](const VoxelShape& in) {
        VoxelShape out = Shapes::empty();
        in.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1) {
            out = Shapes::join(out, Shapes::box(1-z1,y0,x0,1-z0,y1,x1), BooleanOp::Or);
        });
        return out;
    };
    auto rotated = [&](int turns) {
        auto result = quarter;
        for (int i=0; i<turns; ++i) result = rotateCW(result);
        return result;
    };
    VoxelShape upper = quarter;
    if (shape == StairsShape::Straight) {
        upper = Shapes::join(quarter, rotated(1), BooleanOp::Or);
    } else if (shape == StairsShape::InnerLeft || shape == StairsShape::InnerRight) {
        upper = Shapes::join(quarter, rotated(1), BooleanOp::Or);
        upper = Shapes::join(upper, rotated(2), BooleanOp::Or);
        upper = Shapes::join(upper, rotated(3), BooleanOp::Or);
    }
    // Source maps facing for OUTER_RIGHT to clockwise and INNER_LEFT to CCW.
    if (shape == StairsShape::OuterRight) facing = clockwise(facing);
    if (shape == StairsShape::InnerLeft) facing = counterClockwise(facing);
    int turns = facing == Direction::North ? 0 : facing == Direction::East ? 1 :
                facing == Direction::South ? 2 : facing == Direction::West ? 3 : 0;
    VoxelShape oriented = upper;
    for (int i=0; i<turns; ++i) oriented = rotateCW(oriented);
    if (half == StairHalf::Top) {
        // Mirror y coordinates around 1/2.
        VoxelShape mirrored = Shapes::empty();
        oriented.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1) {
            mirrored = Shapes::join(mirrored, Shapes::box(x0,1-y1,z0,x1,1-y0,z1), BooleanOp::Or);
        });
        oriented = mirrored;
        VoxelShape base = Shapes::box(0,0.5,0,1,1,1);
        return Shapes::join(base, oriented, BooleanOp::Or);
    }
    return Shapes::join(Shapes::box(0,0,0,1,0.5,1), oriented, BooleanOp::Or);
}

inline StairHalf stairHalfForPlacement(Direction clickedFace, double hitY, int blockY) {
    return clickedFace != Direction::Down &&
           (clickedFace == Direction::Up || !(hitY - blockY > 0.5))
        ? StairHalf::Bottom : StairHalf::Top;
}

inline StairsShape stairsShapeForPlacement(const PlayableWorld& world, const BlockPos& pos,
                                            Direction facing, StairHalf half) {
    auto sameHalfStairs = [&](const BlockPos& p) {
        return world.getBlock(p) == BlockId::StoneStairs && world.getStairHalf(p) == half;
    };
    const auto behindStep = step(facing);
    const BlockPos behind{pos.x+behindStep.x,pos.y+behindStep.y,pos.z+behindStep.z};
    if (sameHalfStairs(behind)) {
        const Direction behindFacing = world.getStairFacing(behind);
        if (axis(behindFacing) != axis(facing)) {
            const auto check = step(opposite(behindFacing));
            const BlockPos checkPos{pos.x+check.x,pos.y+check.y,pos.z+check.z};
            if (!sameHalfStairs(checkPos) || world.getStairFacing(checkPos) != facing) {
                return behindFacing == counterClockwise(facing) ? StairsShape::OuterLeft : StairsShape::OuterRight;
            }
        }
    }
    const Direction frontFacingDir = opposite(facing);
    const auto frontStep = step(frontFacingDir);
    const BlockPos front{pos.x+frontStep.x,pos.y+frontStep.y,pos.z+frontStep.z};
    if (sameHalfStairs(front)) {
        const Direction neighborFacing = world.getStairFacing(front);
        if (axis(neighborFacing) != axis(facing)) {
            const auto check = step(neighborFacing);
            const BlockPos checkPos{pos.x+check.x,pos.y+check.y,pos.z+check.z};
            if (!sameHalfStairs(checkPos) || world.getStairFacing(checkPos) != facing) {
                return neighborFacing == counterClockwise(facing) ? StairsShape::InnerLeft : StairsShape::InnerRight;
            }
        }
    }
    return StairsShape::Straight;
}

} // namespace carbon
