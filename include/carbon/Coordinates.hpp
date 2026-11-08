#pragma once

// Coordinate arithmetic translated from the supplied Java Edition 26.3 sources:
// net/minecraft/core/{Vec3i,BlockPos,SectionPos}.java and
// net/minecraft/world/level/ChunkPos.java. This is an explicit arithmetic/packing
// subset; serialization, codecs, iterators, mutable positions and gameplay helpers
// are not implemented here.

#include <bit>
#include <cstdint>

namespace carbon {
namespace coordinate_detail {
inline std::int32_t fromBits(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
inline std::int32_t add(std::int32_t a, std::int32_t b) {
    return fromBits(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
inline std::int32_t sub(std::int32_t a, std::int32_t b) {
    return fromBits(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
inline std::int32_t mul(std::int32_t a, std::int32_t b) {
    return fromBits(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
}

struct Vec3i {
    std::int32_t x{}, y{}, z{};
    constexpr Vec3i() = default;
    constexpr Vec3i(std::int32_t x_, std::int32_t y_, std::int32_t z_) : x(x_), y(y_), z(z_) {}
    constexpr std::int32_t getX() const { return x; }
    constexpr std::int32_t getY() const { return y; }
    constexpr std::int32_t getZ() const { return z; }
    constexpr bool operator==(const Vec3i&) const = default;
    std::int32_t hashCode() const {
        return coordinate_detail::add(coordinate_detail::mul(
            coordinate_detail::add(y, coordinate_detail::mul(z, 31)), 31), x);
    }
    std::int32_t compareTo(const Vec3i& p) const {
        if (y != p.y) return coordinate_detail::sub(y, p.y);
        return z == p.z ? coordinate_detail::sub(x, p.x) : coordinate_detail::sub(z, p.z);
    }
    Vec3i offset(std::int32_t dx, std::int32_t dy, std::int32_t dz) const {
        return dx == 0 && dy == 0 && dz == 0 ? *this : Vec3i{
            coordinate_detail::add(x, dx), coordinate_detail::add(y, dy), coordinate_detail::add(z, dz)};
    }
    Vec3i multiply(std::int32_t scale) const {
        if (scale == 1) return *this;
        if (scale == 0) return {};
        return {coordinate_detail::mul(x, scale), coordinate_detail::mul(y, scale), coordinate_detail::mul(z, scale)};
    }
    Vec3i cross(const Vec3i& v) const {
        return {coordinate_detail::sub(coordinate_detail::mul(y, v.z), coordinate_detail::mul(z, v.y)),
                coordinate_detail::sub(coordinate_detail::mul(z, v.x), coordinate_detail::mul(x, v.z)),
                coordinate_detail::sub(coordinate_detail::mul(x, v.y), coordinate_detail::mul(y, v.x))};
    }
};

struct BlockPos : Vec3i {
    using Vec3i::Vec3i;
    static constexpr int PACKED_HORIZONTAL_LENGTH = 26;
    static constexpr int PACKED_Y_LENGTH = 12;
    static constexpr int MAX_HORIZONTAL_COORDINATE = (1 << PACKED_HORIZONTAL_LENGTH) / 2 - 1;
    static constexpr std::uint64_t X_MASK = (1ULL << 26) - 1;
    static constexpr std::uint64_t Y_MASK = (1ULL << 12) - 1;
    static constexpr std::uint64_t Z_MASK = (1ULL << 26) - 1;
    static std::int64_t asLong(std::int32_t x, std::int32_t y, std::int32_t z) {
        const auto bits = ((static_cast<std::uint64_t>(static_cast<std::int64_t>(x)) & X_MASK) << 38)
                       | ((static_cast<std::uint64_t>(static_cast<std::int64_t>(z)) & Z_MASK) << 12)
                       | (static_cast<std::uint64_t>(static_cast<std::int64_t>(y)) & Y_MASK);
        return std::bit_cast<std::int64_t>(bits);
    }
    static std::int32_t getX(std::int64_t packed) {
        return static_cast<std::int32_t>(std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(packed) << 0) >> 38);
    }
    static std::int32_t getY(std::int64_t packed) {
        return static_cast<std::int32_t>(std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(packed) << 52) >> 52);
    }
    static std::int32_t getZ(std::int64_t packed) {
        return static_cast<std::int32_t>(std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(packed) << 26) >> 38);
    }
    static BlockPos of(std::int64_t packed) { return {getX(packed), getY(packed), getZ(packed)}; }
    std::int64_t asLong() const {
        const auto& base = static_cast<const Vec3i&>(*this);
        return asLong(base.getX(), base.getY(), base.getZ());
    }
    static std::int64_t offset(std::int64_t packed, int dx, int dy, int dz) {
        return asLong(coordinate_detail::add(getX(packed), dx), coordinate_detail::add(getY(packed), dy), coordinate_detail::add(getZ(packed), dz));
    }
};

struct SectionPos : Vec3i {
    using Vec3i::Vec3i;
    static constexpr int SECTION_SIZE = 16;
    static std::int32_t blockToSectionCoord(std::int32_t c) { return c >> 4; }
    static std::int32_t sectionRelative(std::int32_t c) { return c & 15; }
    static std::int32_t sectionToBlockCoord(std::int32_t c) { return coordinate_detail::fromBits(static_cast<std::uint32_t>(c) << 4); }
    static std::int32_t sectionToBlockCoord(std::int32_t c, std::int32_t offset) { return coordinate_detail::add(sectionToBlockCoord(c), offset); }
    static std::int64_t asLong(std::int32_t x, std::int32_t y, std::int32_t z) {
        const auto bits = ((static_cast<std::uint64_t>(static_cast<std::int64_t>(x)) & 0x3fffffULL) << 42)
                       | ((static_cast<std::uint64_t>(static_cast<std::int64_t>(z)) & 0x3fffffULL) << 20)
                       | (static_cast<std::uint64_t>(static_cast<std::int64_t>(y)) & 0xfffffULL);
        return std::bit_cast<std::int64_t>(bits);
    }
    static std::int32_t x(std::int64_t packed) { return static_cast<std::int32_t>(packed >> 42); }
    static std::int32_t y(std::int64_t packed) { return static_cast<std::int32_t>(std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(packed) << 44) >> 44); }
    static std::int32_t z(std::int64_t packed) { return static_cast<std::int32_t>(std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(packed) << 22) >> 42); }
    static SectionPos of(std::int64_t packed) { return {x(packed), y(packed), z(packed)}; }
    std::int64_t asLong() const { return asLong(getX(), getY(), getZ()); }
    static std::int16_t sectionRelativePos(const BlockPos& p) {
        const auto v = (sectionRelative(p.x) << 8) | (sectionRelative(p.z) << 4) | sectionRelative(p.y);
        return static_cast<std::int16_t>(v);
    }
    static std::int32_t sectionRelativeX(std::int16_t rel) { return (static_cast<std::uint16_t>(rel) >> 8) & 15; }
    static std::int32_t sectionRelativeY(std::int16_t rel) { return static_cast<std::uint16_t>(rel) & 15; }
    static std::int32_t sectionRelativeZ(std::int16_t rel) { return (static_cast<std::uint16_t>(rel) >> 4) & 15; }
    BlockPos relativeToBlockPos(std::int16_t rel) const {
        return {sectionToBlockCoord(getX(), sectionRelativeX(rel)), sectionToBlockCoord(getY(), sectionRelativeY(rel)), sectionToBlockCoord(getZ(), sectionRelativeZ(rel))};
    }
};

struct ChunkPos {
    std::int32_t x{}, z{};
    constexpr ChunkPos() = default;
    constexpr ChunkPos(std::int32_t x_, std::int32_t z_) : x(x_), z(z_) {}
    constexpr bool operator==(const ChunkPos&) const = default;
    static std::int64_t pack(std::int32_t x, std::int32_t z) {
        return std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<std::int64_t>(x)) & 0xffffffffULL)
            | ((static_cast<std::uint64_t>(static_cast<std::int64_t>(z)) & 0xffffffffULL) << 32));
    }
    std::int64_t pack() const { return pack(x, z); }
    static std::int32_t getX(std::int64_t p) { return static_cast<std::int32_t>(p); }
    static std::int32_t getZ(std::int64_t p) { return static_cast<std::int32_t>(p >> 32); }
    static ChunkPos unpack(std::int64_t p) { return {getX(p), getZ(p)}; }
    static std::int32_t hash(std::int32_t x, std::int32_t z) {
        const auto xt = coordinate_detail::add(coordinate_detail::mul(1664525, x), 1013904223);
        const auto zt = coordinate_detail::add(coordinate_detail::mul(1664525, z ^ -559038737), 1013904223);
        return xt ^ zt;
    }
    std::int32_t hashCode() const { return hash(x, z); }
    std::int32_t getRegionX() const { return x >> 5; }
    std::int32_t getRegionZ() const { return z >> 5; }
    std::int32_t getRegionLocalX() const { return x & 31; }
    std::int32_t getRegionLocalZ() const { return z & 31; }
    std::int32_t getMinBlockX() const { return SectionPos::sectionToBlockCoord(x); }
    std::int32_t getMinBlockZ() const { return SectionPos::sectionToBlockCoord(z); }
    std::int32_t getBlockX(std::int32_t offset) const { return SectionPos::sectionToBlockCoord(x, offset); }
    std::int32_t getBlockZ(std::int32_t offset) const { return SectionPos::sectionToBlockCoord(z, offset); }
};
}
