#include "carbon/PlayableWorld.hpp"
#include "carbon/SlabBlock.hpp"

#include <cassert>
#include <cmath>

int main() {
    using namespace carbon;
    const auto& states = stoneSlabStateDefinition();
    assert(states.getPossibleStates().size() == 3);
    assert(slabTypeProperty()->name() == "type");
    assert(slabTypeProperty()->getName(SlabType::Double) == "double");
    assert(slabVoxelShape(SlabType::Bottom).bounds().maxY == 0.5);
    assert(slabVoxelShape(SlabType::Top).bounds().minY == 0.5);
    assert(slabVoxelShape(SlabType::Double).bounds().maxY == 1.0);

    // Direct source thresholds in SlabBlock.getStateForPlacement and
    // canBeReplaced: the half-height comparison is strictly greater-than.
    assert(slabTypeForPlacement(Direction::Down, 0.9, 0) == SlabType::Top);
    assert(slabTypeForPlacement(Direction::Up, 0.1, 0) == SlabType::Bottom);
    assert(slabTypeForPlacement(Direction::North, 0.5, 0) == SlabType::Bottom);
    assert(slabTypeForPlacement(Direction::North, 0.5001, 0) == SlabType::Top);
    assert(slabCanReplace(SlabType::Bottom, Direction::Up, 0.25, 0, true));
    assert(!slabCanReplace(SlabType::Bottom, Direction::North, 0.5, 0, true));
    assert(slabCanReplace(SlabType::Bottom, Direction::North, 0.5001, 0, true));
    assert(slabCanReplace(SlabType::Top, Direction::North, 0.4999, 0, true));
    assert(!slabCanReplace(SlabType::Double, Direction::Down, 0.1, 0, true));

    auto world = PlayableWorld::createDefaultFlat();
    world.setSlab({0,-61,0}, SlabType::Bottom);
    // A downward ray hits the top face of the bottom slab and merges to double.
    world.player.position = {0.5,-59.5,0.5};
    world.player.pitch = 90.0F;
    const auto hit = world.lookAtBlock(4.0);
    assert(hit && hit->block == BlockPos(0,-61,0));
    assert(hit->face == Direction::Up);
    assert(world.placeTarget(BlockId::StoneSlab));
    assert(world.getSlabType({0,-61,0}) == SlabType::Double);

    // Partial-shape targeting ignores the unoccupied upper half of a bottom slab.
    auto rayWorld = PlayableWorld::createDefaultFlat();
    rayWorld.setSlab({0,-61,0}, SlabType::Bottom);
    auto partialHit = rayWorld.raycast({0.5,-60.25,-1.0}, {0,0,1}, 3.0);
    assert(!partialHit || partialHit->block != BlockPos(0,-61,0));
    partialHit = rayWorld.raycast({0.5,-60.75,-1.0}, {0,0,1}, 3.0);
    assert(partialHit && partialHit->block == BlockPos(0,-61,0));

    // Partial collision permits flying through the open upper half and clips
    // movement at the occupied lower half.
    auto collision = PlayableWorld::createDefaultFlat();
    collision.setSlab({0,0,0}, SlabType::Bottom);
    collision.player.position = {-1.0,0.6,0.5};
    MoveInput right; right.strafe = -1.0F; // yaw zero: negative strafe moves +X
    for (int i=0;i<30;++i) collision.tickCreativeMove(right);
    assert(collision.player.position.x > 0.3);
    collision.player.position = {-1.0,0.0,0.5};
    collision.tickCreativeMove(right);
    assert(collision.player.position.x < 0.1);
}
