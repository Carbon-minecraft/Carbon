#pragma once

// Focused immutable hit-result subset from Java Edition 26.3
// net.minecraft.world.phys.{HitResult,BlockHitResult}. Entity distance and
// network codecs are omitted because this client slice has no entity layer.
#include "carbon/Coordinates.hpp"
#include "carbon/Direction.hpp"
#include "carbon/Vec3.hpp"

namespace carbon {

enum class HitType { Miss, Block, Entity };

class HitResult {
public:
    virtual ~HitResult() = default;
    HitType getType() const noexcept { return type_; }
    const Vec3& getLocation() const noexcept { return location_; }

protected:
    HitResult(HitType type, Vec3 location) : type_(type), location_(location) {}

private:
    HitType type_;
    Vec3 location_;
};

class BlockHitResult final : public HitResult {
public:
    static BlockHitResult miss(Vec3 location, Direction direction, BlockPos pos) {
        return BlockHitResult(HitType::Miss, location, direction, pos, false, false);
    }

    BlockHitResult(Vec3 location, Direction direction, BlockPos pos, bool inside)
        : BlockHitResult(location, direction, pos, inside, false) {}

    BlockHitResult(Vec3 location, Direction direction, BlockPos pos, bool inside, bool worldBorderHit)
        : BlockHitResult(HitType::Block, location, direction, pos, inside, worldBorderHit) {}

    BlockHitResult withDirection(Direction direction) const {
        return BlockHitResult(getType(), getLocation(), direction, blockPos_, inside_, worldBorderHit_);
    }

    BlockHitResult withPosition(BlockPos pos) const {
        return BlockHitResult(getType(), getLocation(), direction_, pos, inside_, worldBorderHit_);
    }

    BlockHitResult hitBorder() const {
        return BlockHitResult(getType(), getLocation(), direction_, blockPos_, inside_, true);
    }

    const BlockPos& getBlockPos() const noexcept { return blockPos_; }
    Direction getDirection() const noexcept { return direction_; }
    bool isInside() const noexcept { return inside_; }
    bool isWorldBorderHit() const noexcept { return worldBorderHit_; }

private:
    BlockHitResult(HitType type, Vec3 location, Direction direction, BlockPos pos,
                   bool inside, bool worldBorderHit)
        : HitResult(type, location), direction_(direction), blockPos_(pos),
          inside_(inside), worldBorderHit_(worldBorderHit) {}

    Direction direction_;
    BlockPos blockPos_;
    bool inside_;
    bool worldBorderHit_;
};

} // namespace carbon
