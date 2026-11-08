#include "carbon/HitResult.hpp"

#include <cassert>

int main() {
    using namespace carbon;
    const Vec3 location{1.25, -2.0, 3.5};
    const BlockPos pos{-4, 5, 6};
    const auto hit = BlockHitResult(location, Direction::East, pos, true);
    assert(hit.getType() == HitType::Block);
    assert(hit.getLocation().x == location.x && hit.getLocation().y == location.y && hit.getLocation().z == location.z);
    assert(hit.getBlockPos() == pos);
    assert(hit.getDirection() == Direction::East);
    assert(hit.isInside());
    assert(!hit.isWorldBorderHit());

    const auto miss = BlockHitResult::miss(location, Direction::Up, pos);
    assert(miss.getType() == HitType::Miss);
    assert(miss.getBlockPos() == pos);
    assert(!miss.isInside() && !miss.isWorldBorderHit());
    const auto turned = miss.withDirection(Direction::North);
    assert(turned.getType() == HitType::Miss);
    assert(turned.getDirection() == Direction::North);
    assert(turned.getBlockPos() == pos);
    const auto moved = turned.withPosition({7, 8, 9});
    assert(moved.getBlockPos() == BlockPos(7, 8, 9));
    assert(moved.getDirection() == Direction::North);
    assert(moved.getLocation().x == location.x);
    const auto border = hit.hitBorder();
    assert(border.isWorldBorderHit());
    assert(!hit.isWorldBorderHit());
    assert(border.getType() == hit.getType() && border.getBlockPos() == pos);
}
