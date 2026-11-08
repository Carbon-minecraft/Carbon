#pragma once

// Source-faithful value equivalent of LevelHeightAccessor.create(minY,height).

#include <cstdint>
#include "carbon/Coordinates.hpp"

namespace carbon {

class LevelHeightAccessor {
public:
    constexpr LevelHeightAccessor(std::int32_t minY, std::int32_t height) noexcept
        : minY_(minY), height_(height) {}

    static constexpr LevelHeightAccessor create(std::int32_t minY, std::int32_t height) noexcept {
        return LevelHeightAccessor(minY, height);
    }

    constexpr std::int32_t getHeight() const noexcept { return height_; }
    constexpr std::int32_t getMinY() const noexcept { return minY_; }
    std::int32_t getMaxY() const noexcept;
    std::int32_t getSectionsCount() const noexcept;
    std::int32_t getMinSectionY() const noexcept;
    std::int32_t getMaxSectionY() const noexcept;
    bool isInsideBuildHeight(std::int32_t blockY) const noexcept;
    bool isInsideBuildHeight(const BlockPos& pos) const noexcept { return isInsideBuildHeight(pos.y); }
    bool isOutsideBuildHeight(std::int32_t blockY) const noexcept;
    bool isOutsideBuildHeight(const BlockPos& pos) const noexcept { return isOutsideBuildHeight(pos.y); }
    std::int32_t getSectionIndex(std::int32_t blockY) const noexcept;
    std::int32_t getSectionIndexFromSectionY(std::int32_t sectionY) const noexcept;
    std::int32_t getSectionYFromSectionIndex(std::int32_t sectionIndex) const noexcept;

private:
    std::int32_t minY_;
    std::int32_t height_;
};

}
