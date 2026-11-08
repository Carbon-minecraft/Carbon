#include "carbon/AABB.hpp"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using carbon::AABB;
    using carbon::Vec3;
    const AABB box{0, 0, 0, 2, 3, 4};
    assert(box.getXsize() == 2 && box.getYsize() == 3 && box.getZsize() == 4);
    assert(box.getSize() == 3);
    assert(box.contains(0, 0, 0));
    assert(!box.contains(2, 1, 1)); // max faces are excluded
    assert(box.intersects(AABB{1, 1, 1, 3, 4, 5}));
    assert(!box.intersects(AABB{2, 0, 0, 3, 1, 1})); // touching faces do not overlap
    assert(!box.intersects(AABB{2, 3, 4, 5, 6, 7})); // touching corner does not overlap
    assert(box.intersect(AABB{2, -1, 1, 5, 2, 8}).getXsize() == 0);
    assert(box.distanceToSqr(Vec3{3, 5, 4}) == 5);
    assert(box.distanceToSqr(AABB{4, 5, 4, 5, 6, 5}) == 8);

    const auto negative = AABB{4, 3, 2, -2, -1, 0};
    assert(negative.minX == -2 && negative.minY == -1 && negative.minZ == 0);
    assert(negative.maxX == 4 && negative.maxY == 3 && negative.maxZ == 2);
    const auto degenerate = AABB{1, 2, 3, 1, 2, 3};
    assert(degenerate.getSize() == 0 && !degenerate.contains(1, 2, 3));
    // Java intersects uses strict overlap comparisons but allows a point-sized
    // box when its coordinate lies strictly inside the other box.
    assert(degenerate.intersects(AABB{0, 0, 0, 2, 4, 4}));

    const auto moved = box.move(-1, 2, -3);
    assert(moved.minX == -1 && moved.minY == 2 && moved.minZ == -3);
    assert(moved.maxX == 1 && moved.maxY == 5 && moved.maxZ == 1);
    const auto contracted = box.contract(-1, 2, 0);
    assert(contracted.minX == 1 && contracted.maxX == 2 && contracted.minY == 0 && contracted.maxY == 1);
    const auto grown = box.expandTowards(Vec3{-2, 1, 0});
    assert(grown.minX == -2 && grown.maxX == 2 && grown.maxY == 4);
    const auto inflated = box.inflate(1, 2, 3);
    assert(inflated.minX == -1 && inflated.minY == -2 && inflated.minZ == -3);
    const auto deflated = box.deflate(1);
    assert(deflated.minX == 1 && deflated.maxX == 1 && deflated.minY == 1 && deflated.maxY == 2);

    const auto hit = box.clip(Vec3{-1, 1, 2}, Vec3{3, 1, 2});
    assert(hit && hit->x == 0 && hit->y == 1 && hit->z == 2);
    assert(!box.clip(Vec3{-1, 5, 2}, Vec3{3, 5, 2}));
    // The Java helper only tests the direction-facing entry plane, so a
    // segment beginning inside does not report the exit face.
    assert(!box.clip(Vec3{1, 1, 2}, Vec3{3, 1, 2}));

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto nanBox = AABB{nan, 0, 0, 1, 1, 1};
    assert(std::isnan(nanBox.minX) && std::isnan(nanBox.maxX));
    const auto signedZero = AABB{0.0, -0.0, 0, -0.0, 0.0, 1};
    assert(std::signbit(signedZero.minX) && !std::signbit(signedZero.maxX));
    assert(std::signbit(signedZero.minY) && !std::signbit(signedZero.maxY));
    const auto edgeBlock = AABB{carbon::BlockPos{INT32_MAX, 0, 0}};
    assert(edgeBlock.minX == static_cast<double>(INT32_MIN));
    assert(edgeBlock.maxX == static_cast<double>(INT32_MAX)); // Java int + 1 wraps before widening
}
