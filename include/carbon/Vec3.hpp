#pragma once

// Arithmetic subset of net.minecraft.world.phys.Vec3 (Java Edition 26.3).
// Components are doubles and operations intentionally follow source expression
// order. Codec, direction, random, and JOML integrations are outside this subset.

#include "carbon/Coordinates.hpp"
#include "carbon/Math.hpp"

#include <cmath>

namespace carbon {
struct Vec3 {
    double x{}, y{}, z{};
    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    explicit constexpr Vec3(const Vec3i& v) : x(v.getX()), y(v.getY()), z(v.getZ()) {}

    Vec3 vectorTo(const Vec3& v) const { return {v.x - x, v.y - y, v.z - z}; }
    Vec3 normalize() const {
        const double dist = std::sqrt(x * x + y * y + z * z);
        return dist < 1.0E-5F ? Vec3{} : Vec3{x / dist, y / dist, z / dist};
    }
    double dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    Vec3 cross(const Vec3& v) const { return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x}; }
    Vec3 add(double dx, double dy, double dz) const { return {x + dx, y + dy, z + dz}; }
    Vec3 add(double v) const { return add(v, v, v); }
    Vec3 add(const Vec3& v) const { return add(v.x, v.y, v.z); }
    Vec3 subtract(double dx, double dy, double dz) const { return add(-dx, -dy, -dz); }
    Vec3 subtract(double v) const { return subtract(v, v, v); }
    Vec3 subtract(const Vec3& v) const { return subtract(v.x, v.y, v.z); }
    Vec3 scale(double v) const { return multiply(v, v, v); }
    Vec3 multiply(double sx, double sy, double sz) const { return {x * sx, y * sy, z * sz}; }
    Vec3 multiply(const Vec3& v) const { return multiply(v.x, v.y, v.z); }
    Vec3 reverse() const { return scale(-1.0); }
    Vec3 horizontal() const { return {x, 0.0, z}; }
    double length() const { return std::sqrt(x * x + y * y + z * z); }
    double lengthSqr() const { return x * x + y * y + z * z; }
    double horizontalDistance() const { return std::sqrt(x * x + z * z); }
    double horizontalDistanceSqr() const { return x * x + z * z; }
    double distanceToSqr(const Vec3& v) const {
        const double xd = v.x - x, yd = v.y - y, zd = v.z - z;
        return xd * xd + yd * yd + zd * zd;
    }
    double distanceTo(const Vec3& v) const {
        const double xd = v.x - x, yd = v.y - y, zd = v.z - z;
        return std::sqrt(xd * xd + yd * yd + zd * zd);
    }
    Vec3 lerp(const Vec3& v, double a) const {
        return {Mth::lerp(a, x, v.x), Mth::lerp(a, y, v.y), Mth::lerp(a, z, v.z)};
    }
    bool isFinite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
};
}
