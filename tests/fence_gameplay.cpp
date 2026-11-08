#include "carbon/FenceBlock.hpp"
#include "carbon/PlayableWorld.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace carbon;

int main() {
    const auto& definition = oakFenceStateDefinition();
    assert(definition.getPossibleStates().size() == 16);
    const auto& props = definition.getPossibleStates().front()->getProperties();
    assert(props.size() == 4);
    assert(props[0]->name() == "east" && props[1]->name() == "north" &&
           props[2]->name() == "south" && props[3]->name() == "west");
    for (unsigned mask=0; mask<16; ++mask) {
        FenceConnections c{bool(mask&1),bool(mask&2),bool(mask&4),bool(mask&8)};
        const auto outline = fenceOutlineShape(c).bounds();
        const auto collision = fenceCollisionShape(c).bounds();
        assert(std::abs(outline.minY) < 1e-9 && std::abs(outline.maxY-1.0) < 1e-9);
        assert(std::abs(collision.minY) < 1e-9 && std::abs(collision.maxY-1.5) < 1e-9);
        assert(outline.minX == (c.west ? 0.0 : 0.375));
        assert(outline.maxX == (c.east ? 1.0 : 0.625));
        assert(outline.minZ == (c.north ? 0.0 : 0.375));
        assert(outline.maxZ == (c.south ? 1.0 : 0.625));
        assert(std::abs(collision.minX-outline.minX)<1e-9 && std::abs(collision.maxZ-outline.maxZ)<1e-9);
    }

    auto world = PlayableWorld::createDefaultFlat();
    const BlockPos fence{5,10,-3};
    world.setFence(fence);
    assert(world.getFenceConnections(fence) == FenceConnections{});
    world.setBlock({6,10,-3},BlockId::Stone);
    assert(world.getFenceConnections(fence).east);
    world.setBlock({6,10,-3},BlockId::Air);
    assert(!world.getFenceConnections(fence).east);
    world.setFence({6,10,-3});
    assert(world.getFenceConnections(fence).east);
    world.setBlock({6,10,-3},BlockId::Air);
    assert(!world.getFenceConnections(fence).east);

    // Source FULL face support: half slabs do not fill a side, doubled slabs do.
    world.setSlab({4,10,-3},SlabType::Bottom);
    assert(!world.getFenceConnections(fence).west);
    world.setSlab({4,10,-3},SlabType::Top);
    assert(!world.getFenceConnections(fence).west);
    world.setSlab({4,10,-3},SlabType::Double);
    assert(world.getFenceConnections(fence).west);
    world.setBlock({4,10,-3},BlockId::Air);
    assert(!world.getFenceConnections(fence).west);

    // Stair side faces are shape-sensitive and are not promoted to cubes.
    world.setStairs({5,10,-4},Direction::North,StairHalf::Bottom,StairsShape::Straight);
    const bool northStairFullFace = false; // straight stair's north support face is partial in Java
    assert(world.getFenceConnections(fence).north == northStairFullFace);
    world.setStairs({5,10,-4},Direction::South,StairHalf::Bottom,StairsShape::Straight);
    assert(world.getFenceConnections(fence).north); // the source stair has a full south side face
    world.setBlock({5,10,-4},BlockId::Air);
    assert(!world.getFenceConnections(fence).north);
    world.setBlock({5,10,-4},BlockId::Stone);
    assert(world.getFenceConnections(fence).north);

    // Placement reads neighboring support faces and produces a connected state.
    auto placementWorld = PlayableWorld::createDefaultFlat();
    placementWorld.setBlock({0,10,3},BlockId::Stone);
    placementWorld.player.position = {0.5,9.0,0.5};
    placementWorld.player.yaw = 0.0F;
    placementWorld.player.pitch = 0.0F;
    assert(placementWorld.placeTarget(BlockId::OakFence));
    assert(placementWorld.getFenceConnections({0,10,2}).south);

    auto breakingWorld = PlayableWorld::createDefaultFlat();
    breakingWorld.setFence({0,10,1});
    breakingWorld.setFence({0,10,2});
    assert(breakingWorld.getFenceConnections({0,10,2}).north);
    breakingWorld.player.position = {0.5,9.0,-2.0};
    breakingWorld.player.yaw = 0.0F;
    breakingWorld.player.pitch = 0.0F;
    assert(breakingWorld.breakTarget());
    assert(breakingWorld.getBlock({0,10,1}) == BlockId::Air);
    assert(!breakingWorld.getFenceConnections({0,10,2}).north);

    // Ray picking uses the 1-block outline; collision uses the 1.5-block shape.
    world.setBlock(fence,BlockId::Air);
    world.setFence(fence);
    assert(!world.raycast({5.5,11.25,-4.0},{0,0,1},2.0));
    world.player.position = {5.5,11.25,-4.0};
    world.player.yaw = 0.0F;
    // This elevated collision check intentionally flies over the lower outline
    // shape while overlapping the connected north arm's taller collision box.
    // The world has not moved the player yet, so its vertical velocity is zero.
    world.player.flying = true;
    world.player.onGround = false;
    MoveInput forward; forward.forward = 1.0F;
    for(int i=0;i<100;++i) world.tickCreativeMove(forward);
    const double connectedArmBoundary = static_cast<double>(fence.z);
    const double expectedFenceContact = connectedArmBoundary - PlayableWorld::PLAYER_WIDTH / 2.0;
    assert(std::abs(world.player.position.y - 11.25) < 1e-9);
    assert(std::abs(world.player.position.z - expectedFenceContact) < 1e-6);

    std::cout << "fence gameplay tests passed (native assertions; no JVM oracle run)\n";
}
