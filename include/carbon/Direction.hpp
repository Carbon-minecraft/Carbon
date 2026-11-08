#pragma once

// Dependency-free Direction/Axis subset from Java Edition 26.3
// net.minecraft.core.Direction. Codec, random, entity, Vec3f/JOML and plane
// iteration helpers are intentionally outside this port.

#include "carbon/Coordinates.hpp"
#include "carbon/Math.hpp"
#include "carbon/Vec3.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace carbon {

enum class Axis : std::uint8_t { X, Y, Z };
enum class AxisDirection : std::uint8_t { Positive, Negative };

enum class Direction : std::uint8_t { Down, Up, North, South, West, East };

struct DirectionInfo {
    int data3d;
    int oppositeIndex;
    int data2d;
    std::string_view name;
    AxisDirection axisDirection;
    Axis axis;
    Vec3i normal;
};

inline constexpr std::array<Direction, 6> DirectionValues{
    Direction::Down, Direction::Up, Direction::North,
    Direction::South, Direction::West, Direction::East};

inline constexpr DirectionInfo directionInfo(Direction d) {
    switch (d) {
    case Direction::Down:  return {0, 1, -1, "down", AxisDirection::Negative, Axis::Y, {0,-1,0}};
    case Direction::Up:    return {1, 0, -1, "up", AxisDirection::Positive, Axis::Y, {0,1,0}};
    case Direction::North: return {2, 3, 2, "north", AxisDirection::Negative, Axis::Z, {0,0,-1}};
    case Direction::South: return {3, 2, 0, "south", AxisDirection::Positive, Axis::Z, {0,0,1}};
    case Direction::West:  return {4, 5, 1, "west", AxisDirection::Negative, Axis::X, {-1,0,0}};
    case Direction::East:  return {5, 4, 3, "east", AxisDirection::Positive, Axis::X, {1,0,0}};
    }
    return {0,1,-1,"down",AxisDirection::Negative,Axis::Y,{0,-1,0}};
}

inline constexpr Axis axis(Direction d) { return directionInfo(d).axis; }
inline constexpr int data3d(Direction d) { return directionInfo(d).data3d; }
inline constexpr int data2d(Direction d) { return directionInfo(d).data2d; }
inline constexpr std::string_view name(Direction d) { return directionInfo(d).name; }
inline constexpr Vec3i step(Direction d) { return directionInfo(d).normal; }
inline constexpr Direction opposite(Direction d) {
    switch (d) {
    case Direction::Down: return Direction::Up; case Direction::Up: return Direction::Down;
    case Direction::North: return Direction::South; case Direction::South: return Direction::North;
    case Direction::West: return Direction::East; case Direction::East: return Direction::West;
    }
    return Direction::Down;
}
inline constexpr Direction fromAxisAndDirection(Axis a, AxisDirection s) {
    if (a == Axis::X) return s == AxisDirection::Positive ? Direction::East : Direction::West;
    if (a == Axis::Y) return s == AxisDirection::Positive ? Direction::Up : Direction::Down;
    return s == AxisDirection::Positive ? Direction::South : Direction::North;
}
inline constexpr Direction axisPositive(Axis a) { return fromAxisAndDirection(a, AxisDirection::Positive); }
inline constexpr Direction axisNegative(Axis a) { return fromAxisAndDirection(a, AxisDirection::Negative); }
inline constexpr AxisDirection opposite(AxisDirection d) { return d == AxisDirection::Positive ? AxisDirection::Negative : AxisDirection::Positive; }
inline constexpr bool isVertical(Axis a) { return a == Axis::Y; }
inline constexpr bool isHorizontal(Axis a) { return a != Axis::Y; }
inline constexpr std::optional<Axis> axisByName(std::string_view s) {
    if (s == "x") return Axis::X; if (s == "y") return Axis::Y; if (s == "z") return Axis::Z; return std::nullopt;
}
inline constexpr std::optional<Direction> directionByName(std::string_view s) {
    for (auto d : DirectionValues) if (name(d) == s) return d;
    return std::nullopt;
}
inline constexpr Direction from3DDataValue(std::int32_t data) {
    // Java: BY_3D_DATA[abs(data % 6)]. This also preserves MIN_VALUE behavior.
    auto r = data % 6;
    if (r < 0) r = -r;
    constexpr std::array<Direction,6> byId{Direction::Down,Direction::Up,Direction::North,Direction::South,Direction::West,Direction::East};
    return byId[static_cast<std::size_t>(r)];
}
// Direction.BY_ID uses ByIdMap's WRAP strategy (positive modulo), distinct
// from the legacy from3DDataValue method's absolute signed remainder.
inline constexpr Direction directionById(std::int32_t id) {
    auto r = id % 6; if (r < 0) r += 6;
    return DirectionValues[static_cast<std::size_t>(r)];
}
inline constexpr Direction from2DDataValue(std::int32_t data) {
    auto r = data % 4; if (r < 0) r = -r;
    constexpr std::array<Direction,4> byId{Direction::South,Direction::West,Direction::North,Direction::East};
    return byId[static_cast<std::size_t>(r)];
}
inline Direction fromYRot(double yRot) {
    const auto floored = Mth::floor(yRot / 90.0 + 0.5);
    return from2DDataValue(floored & 3);
}
inline constexpr float toYRot(Direction d) { return static_cast<float>((data2d(d) & 3) * 90); }
inline float getYRot(Direction d) {
    switch (d) { case Direction::North:return 180.0F; case Direction::South:return 0.0F;
    case Direction::West:return 90.0F; case Direction::East:return -90.0F;
    default: throw std::logic_error("No y-Rot for vertical axis"); }
}
inline constexpr Direction clockwise(Direction d) {
    switch(d) { case Direction::North:return Direction::East; case Direction::East:return Direction::South; case Direction::South:return Direction::West; case Direction::West:return Direction::North; default:return d; }
}
inline constexpr Direction counterClockwise(Direction d) {
    switch(d) { case Direction::North:return Direction::West; case Direction::West:return Direction::South; case Direction::South:return Direction::East; case Direction::East:return Direction::North; default:return d; }
}
inline constexpr Direction clockwise(Direction d, Axis a) {
    if (a == Axis::Y) return clockwise(d);
    if (a == Axis::X) {
        switch(d) { case Direction::Down:return Direction::South; case Direction::South:return Direction::Up; case Direction::Up:return Direction::North; case Direction::North:return Direction::Down; default:return d; }
    }
    switch(d) { case Direction::Down:return Direction::West; case Direction::West:return Direction::Up; case Direction::Up:return Direction::East; case Direction::East:return Direction::Down; default:return d; }
}
inline constexpr Direction counterClockwise(Direction d, Axis a) {
    if (a == Axis::Y) return counterClockwise(d);
    if (a == Axis::X) {
        switch(d) { case Direction::Down:return Direction::North; case Direction::North:return Direction::Up; case Direction::Up:return Direction::South; case Direction::South:return Direction::Down; default:return d; }
    }
    switch(d) { case Direction::Down:return Direction::East; case Direction::East:return Direction::Up; case Direction::Up:return Direction::West; case Direction::West:return Direction::Down; default:return d; }
}
inline Direction approximateNearest(float dx, float dy, float dz) {
    Direction result = Direction::North;
    float highestDot = std::numeric_limits<float>::min(); // Java Float.MIN_VALUE (positive)
    for (auto d : DirectionValues) {
        const auto n = step(d);
        const float dot = dx * n.x + dy * n.y + dz * n.z;
        if (dot > highestDot) { highestDot = dot; result = d; }
    }
    return result;
}
inline Direction approximateNearest(double dx, double dy, double dz) { return approximateNearest(static_cast<float>(dx),static_cast<float>(dy),static_cast<float>(dz)); }
inline Direction approximateNearest(const Vec3& v) { return approximateNearest(v.x,v.y,v.z); }
inline constexpr Direction nearest(std::int32_t x, std::int32_t y, std::int32_t z, Direction fallback) {
    const auto ax = x == INT32_MIN ? INT32_MIN : (x < 0 ? -x : x);
    const auto ay = y == INT32_MIN ? INT32_MIN : (y < 0 ? -y : y);
    const auto az = z == INT32_MIN ? INT32_MIN : (z < 0 ? -z : z);
    if (ax > az && ax > ay) return x < 0 ? Direction::West : Direction::East;
    if (az > ax && az > ay) return z < 0 ? Direction::North : Direction::South;
    if (ay > ax && ay > az) return y < 0 ? Direction::Down : Direction::Up;
    return fallback;
}
inline constexpr Direction nearest(const Vec3i& p, Direction fallback) { return nearest(p.x,p.y,p.z,fallback); }
inline bool isFacingAngle(Direction d, float yAngle) {
    const float radians = yAngle * static_cast<float>(3.14159265358979323846 / 180.0);
    const float dx = -Mth::sin(radians), dz = Mth::cos(radians);
    const auto n = step(d);
    return n.x * dx + n.z * dz > 0.0F;
}

} // namespace carbon
