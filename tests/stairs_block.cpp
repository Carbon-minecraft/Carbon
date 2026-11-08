#include "carbon\PlayableWorld.hpp"
#include "carbon\StairsBlock.hpp"

#include <cassert>
#include <iostream>

int main() {
    using namespace carbon;
    assert(stoneStairsStateDefinition().getPossibleStates().size() == 40);

    // Source voxel shape table: every horizontal facing and both halves keep
    // a 3/4-block straight profile, with upper/lower occupancy mirrored.
    for (auto facing : {Direction::North,Direction::East,Direction::South,Direction::West}) {
        for (auto half : {StairHalf::Bottom,StairHalf::Top}) {
            auto s = stairsVoxelShape(facing,half,StairsShape::Straight);
            assert(!s.isEmpty());
            const auto b = s.bounds();
            assert(b.minX == 0 && b.minY == 0 && b.minZ == 0);
            assert(b.maxX == 1 && b.maxY == 1 && b.maxZ == 1);
            int boxes = 0;
            s.forAllBoxes([&](double,double,double,double,double,double) { ++boxes; });
            assert(boxes >= 2 && boxes <= 4);
            int occupiedOctants = 0;
            s.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1) {
                for (int x=0;x<2;++x) for (int y=0;y<2;++y) for (int z=0;z<2;++z) {
                    const double px=x*0.5+0.25, py=y*0.5+0.25, pz=z*0.5+0.25;
                    if (px>=x0 && px<x1 && py>=y0 && py<y1 && pz>=z0 && pz<z1) ++occupiedOctants;
                }
            });
            assert(occupiedOctants == 6);
            for (auto shape : {StairsShape::InnerLeft,StairsShape::InnerRight,
                               StairsShape::OuterLeft,StairsShape::OuterRight}) {
                auto shaped = stairsVoxelShape(facing,half,shape);
                occupiedOctants = 0;
                shaped.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1) {
                    for (int x=0;x<2;++x) for (int y=0;y<2;++y) for (int z=0;z<2;++z) {
                        const double px=x*0.5+0.25, py=y*0.5+0.25, pz=z*0.5+0.25;
                        if (px>=x0 && px<x1 && py>=y0 && py<y1 && pz>=z0 && pz<z1) ++occupiedOctants;
                    }
                });
                assert(occupiedOctants == (shape == StairsShape::InnerLeft || shape == StairsShape::InnerRight ? 8 : 5));
            }
        }
    }
    auto northBottom = stairsVoxelShape(Direction::North,StairHalf::Bottom,StairsShape::Straight);
    auto northTop = stairsVoxelShape(Direction::North,StairHalf::Top,StairsShape::Straight);
    assert(northBottom.discrete().isFull(0,0,0));
    assert(!northBottom.discrete().isFull(0,1,1)); // open upper front half
    assert(northTop.discrete().isFull(0,1,0));
    assert(!northTop.discrete().isFull(0,0,1));
    for (auto shape : {StairsShape::InnerLeft,StairsShape::InnerRight,
                       StairsShape::OuterLeft,StairsShape::OuterRight}) {
        assert(!stairsVoxelShape(Direction::South,StairHalf::Bottom,shape).isEmpty());
    }

    // Exact click-height/face boundary inherited from StairBlock placement.
    assert(stairHalfForPlacement(Direction::Up, 0.0, 0) == StairHalf::Bottom);
    assert(stairHalfForPlacement(Direction::North, 0.5, 0) == StairHalf::Bottom);
    assert(stairHalfForPlacement(Direction::North, 0.500001, 0) == StairHalf::Top);
    assert(stairHalfForPlacement(Direction::Down, 0.1, 0) == StairHalf::Top);

    PlayableWorld world;
    world.setStairs({0,10,0},Direction::North,StairHalf::Bottom);
    world.setStairs({0,10,-1},Direction::West,StairHalf::Bottom);
    assert(world.getStairShape({0,10,0}) == StairsShape::OuterLeft);
    world.setBlock({0,10,-1},BlockId::Air); // neighbor break recomputes current stair
    assert(world.getStairShape({0,10,0}) == StairsShape::Straight);
    world.setStairs({0,10,1},Direction::West,StairHalf::Bottom);
    assert(world.getStairShape({0,10,0}) == StairsShape::InnerLeft);
    world.setStairs({-1,10,0},Direction::North,StairHalf::Bottom); // canTakeShape veto
    assert(world.getStairShape({0,10,0}) == StairsShape::Straight);

    // Partial raycast and collision use the voxel union, not its full bounds.
    PlayableWorld rayWorld;
    rayWorld.setStairs({0,10,0},Direction::North,StairHalf::Bottom);
    auto miss = rayWorld.raycast({-1,10.75,0.75},{1,0,0},3);
    assert(!miss);
    auto hit = rayWorld.raycast({-1,10.25,0.75},{1,0,0},3);
    assert(hit && hit->block == BlockPos(0,10,0));
    AABB upperFront{0.7,0.7,0.7,0.9,0.9,0.8};
    assert(northBottom.collide(Axis::X,upperFront,-0.25) == -0.25);
    AABB lowerFront{0.7,0.1,0.7,0.9,0.3,0.8};
    assert(northBottom.collide(Axis::X,lowerFront,-0.25) > -0.25);
    std::cout << "stairs_block: source StairBlock geometry/state/corner/raycast subset passed\n";
}
