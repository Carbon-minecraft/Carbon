#include "carbon/LevelHeightAccessor.hpp"

#include <bit>

namespace carbon {
namespace {
std::int32_t wrapAdd(std::int32_t a, std::int32_t b) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t wrapSub(std::int32_t a, std::int32_t b) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
std::int32_t sectionCoord(std::int32_t blockY) noexcept { return blockY >> 4; }
}

std::int32_t LevelHeightAccessor::getMaxY() const noexcept { return wrapSub(wrapAdd(minY_, height_), 1); }
std::int32_t LevelHeightAccessor::getMinSectionY() const noexcept { return sectionCoord(minY_); }
std::int32_t LevelHeightAccessor::getMaxSectionY() const noexcept { return sectionCoord(getMaxY()); }
std::int32_t LevelHeightAccessor::getSectionsCount() const noexcept { return wrapAdd(wrapSub(getMaxSectionY(), getMinSectionY()), 1); }
bool LevelHeightAccessor::isInsideBuildHeight(std::int32_t y) const noexcept { return y >= minY_ && y <= getMaxY(); }
bool LevelHeightAccessor::isOutsideBuildHeight(std::int32_t y) const noexcept { return y < minY_ || y > getMaxY(); }
std::int32_t LevelHeightAccessor::getSectionIndex(std::int32_t y) const noexcept { return getSectionIndexFromSectionY(sectionCoord(y)); }
std::int32_t LevelHeightAccessor::getSectionIndexFromSectionY(std::int32_t sectionY) const noexcept { return wrapSub(sectionY, getMinSectionY()); }
std::int32_t LevelHeightAccessor::getSectionYFromSectionIndex(std::int32_t sectionIndex) const noexcept { return wrapAdd(sectionIndex, getMinSectionY()); }

}
