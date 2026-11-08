#pragma once

// Block Rotation and Mirror arithmetic from Java Edition 26.3
// net.minecraft.world.level.block.{Rotation,Mirror}. OctahedralGroup/codec,
// localization Component, and random helpers are outside this dependency-free subset.

#include "carbon/Direction.hpp"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace carbon {

enum class Rotation : std::uint8_t { None, Clockwise90, Clockwise180, Counterclockwise90 };
enum class Mirror : std::uint8_t { None, LeftRight, FrontBack };

inline constexpr int rotationIndex(Rotation r) { return static_cast<int>(r); }
inline constexpr const char* rotationName(Rotation r) {
    switch(r) { case Rotation::None:return "none"; case Rotation::Clockwise90:return "clockwise_90";
    case Rotation::Clockwise180:return "180"; case Rotation::Counterclockwise90:return "counterclockwise_90"; }
    return "none";
}
inline constexpr Rotation rotationById(std::int32_t id) {
    auto r = id % 4; if (r < 0) r += 4;
    return static_cast<Rotation>(r);
}
inline constexpr Rotation rotated(Rotation current, Rotation by) {
    return static_cast<Rotation>((rotationIndex(current) + rotationIndex(by)) & 3);
}
inline constexpr Direction rotate(Rotation r, Direction d) {
    if (axis(d) == Axis::Y) return d;
    switch(r) { case Rotation::Clockwise90:return clockwise(d); case Rotation::Clockwise180:return opposite(d);
    case Rotation::Counterclockwise90:return counterClockwise(d); default:return d; }
}
namespace direction_detail {
inline constexpr std::int32_t wrapAdd(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
inline constexpr std::int32_t wrapSub(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
inline constexpr std::int32_t wrapMul(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
}
inline std::int32_t rotate(Rotation r, std::int32_t rotation, std::int32_t steps) {
    if (steps == 0) throw std::domain_error("Java integer division by zero");
    switch(r) {
    case Rotation::Clockwise90: return direction_detail::wrapAdd(rotation, steps / 4) % steps;
    case Rotation::Clockwise180: return direction_detail::wrapAdd(rotation, steps / 2) % steps;
    case Rotation::Counterclockwise90:
        return direction_detail::wrapAdd(rotation, direction_detail::wrapMul(steps, 3) / 4) % steps;
    default: return rotation;
    }
}
inline constexpr const char* mirrorName(Mirror m) {
    switch(m) { case Mirror::None:return "none"; case Mirror::LeftRight:return "left_right"; case Mirror::FrontBack:return "front_back"; }
    return "none";
}
inline std::int32_t mirror(Mirror m, std::int32_t rotation, std::int32_t steps) {
    if (steps == 0) throw std::domain_error("Java integer division by zero");
    const auto halfSteps = steps / 2;
    const auto corrected = rotation > halfSteps ? direction_detail::wrapSub(rotation, steps) : rotation;
    switch(m) {
    case Mirror::LeftRight:
        return direction_detail::wrapAdd(direction_detail::wrapSub(halfSteps, corrected), steps) % steps;
    case Mirror::FrontBack:
        return direction_detail::wrapSub(steps, corrected) % steps;
    default: return rotation;
    }
}
inline constexpr Rotation mirrorRotation(Mirror m, Direction d) {
    return ((m == Mirror::LeftRight && axis(d) == Axis::Z) || (m == Mirror::FrontBack && axis(d) == Axis::X))
        ? Rotation::Clockwise180 : Rotation::None;
}
inline constexpr Direction mirror(Mirror m, Direction d) {
    return ((m == Mirror::FrontBack && axis(d) == Axis::X) || (m == Mirror::LeftRight && axis(d) == Axis::Z))
        ? opposite(d) : d;
}

} // namespace carbon
