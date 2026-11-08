#pragma once

// Dependency-free subset of net.minecraft.util.Mth from the supplied 26.3
// decompilation. Floating point expressions retain Java's source evaluation
// order; casts that Java defines for NaN/out-of-range values are explicit.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace carbon {
struct Mth {
    static constexpr float PI = static_cast<float>(3.14159265358979323846);
    static constexpr float HALF_PI = static_cast<float>(3.14159265358979323846 / 2.0);
    static constexpr float TWO_PI = static_cast<float>(3.14159265358979323846 * 2.0);
    static constexpr float DEG_TO_RAD = static_cast<float>(3.14159265358979323846 / 180.0);
    static constexpr float RAD_TO_DEG = 180.0F / PI;
    static constexpr float EPSILON = 1.0E-5F;

private:
    static std::int32_t javaInt(double v) {
        if (std::isnan(v)) return 0;
        if (v >= 2147483647.0) return INT32_MAX;
        if (v <= -2147483648.0) return INT32_MIN;
        return static_cast<std::int32_t>(v);
    }
    static std::int64_t javaLong(double v) {
        if (std::isnan(v)) return 0;
        if (v >= 9223372036854775808.0) return INT64_MAX;
        if (v <= -9223372036854775808.0) return INT64_MIN;
        return static_cast<std::int64_t>(v);
    }
    static const std::array<float, 65536>& sinTable() {
        static const auto table = [] {
            std::array<float, 65536> values{};
            for (std::size_t i = 0; i < values.size(); ++i)
                values[i] = static_cast<float>(std::sin(static_cast<double>(i) / 10430.378350470453));
            return values;
        }();
        return table;
    }
    static std::uint16_t sinIndex(double value) {
        // Java casts double to long with saturation, then masks the low 16 bits.
        const auto scaled = javaLong(value * 10430.378350470453);
        return static_cast<std::uint16_t>(static_cast<std::uint64_t>(scaled) & 65535ULL);
    }
    static float javaMin(float a, float b) {
        if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<float>::quiet_NaN();
        if (a == b) return std::signbit(a) ? a : b;
        return a < b ? a : b;
    }
    static double javaMin(double a, double b) {
        if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<double>::quiet_NaN();
        if (a == b) return std::signbit(a) ? a : b;
        return a < b ? a : b;
    }
public:
    static float sin(double i) { return sinTable()[sinIndex(i)]; }
    // Source cos indexes using (long)(i * scale + 16384), separately from
    // sin's conversion to preserve the exact source rounding point.
    static float cos(double i) {
        const auto scaled = javaLong(i * 10430.378350470453 + 16384.0);
        return sinTable()[static_cast<std::uint16_t>(static_cast<std::uint64_t>(scaled) & 65535ULL)];
    }
    static float sqrt(float x) { return static_cast<float>(std::sqrt(x)); }
    static std::int32_t floor(float v) { return javaInt(std::floor(static_cast<double>(v))); }
    static std::int32_t floor(double v) { return javaInt(std::floor(v)); }
    static std::int64_t lfloor(double v) { return javaLong(std::floor(v)); }
    static std::int32_t ceil(float v) { return javaInt(std::ceil(static_cast<double>(v))); }
    static std::int32_t ceil(double v) { return javaInt(std::ceil(v)); }
    static std::int64_t ceilLong(double v) { return javaLong(std::ceil(v)); }
    static float abs(float v) { return std::fabs(v); }
    static std::int32_t abs(std::int32_t v) { return v == INT32_MIN ? v : (v < 0 ? -v : v); }
    static std::int32_t clamp(std::int32_t v, std::int32_t lo, std::int32_t hi) { return std::min(std::max(v, lo), hi); }
    static std::int64_t clamp(std::int64_t v, std::int64_t lo, std::int64_t hi) { return std::min(std::max(v, lo), hi); }
    static float clamp(float v, float lo, float hi) { return v < lo ? lo : javaMin(v, hi); }
    static double clamp(double v, double lo, double hi) { return v < lo ? lo : javaMin(v, hi); }
    static float lerp(float a, float p0, float p1) {
        const float result = p0 + a * (p1 - p0);
        return std::isnan(result) ? std::numeric_limits<float>::quiet_NaN() : result;
    }
    static double lerp(double a, double p0, double p1) {
        const double result = p0 + a * (p1 - p0);
        return std::isnan(result) ? std::numeric_limits<double>::quiet_NaN() : result;
    }
    static float clampedLerp(float a, float lo, float hi) { return a < 0.0F ? lo : (a > 1.0F ? hi : lerp(a, lo, hi)); }
    static double clampedLerp(double a, double lo, double hi) { return a < 0.0 ? lo : (a > 1.0 ? hi : lerp(a, lo, hi)); }
    static double inverseLerp(double v, double lo, double hi) { return (v - lo) / (hi - lo); }
    static float inverseLerp(float v, float lo, float hi) { return (v - lo) / (hi - lo); }
    static std::int32_t wrapDegrees(std::int32_t angle) {
        auto a = angle % 360;
        if (a >= 180) a -= 360;
        if (a < -180) a += 360;
        return a;
    }
    static float wrapDegrees(float angle) {
        if (std::isnan(angle)) return std::numeric_limits<float>::quiet_NaN();
        if (std::isinf(angle)) return std::copysign(std::numeric_limits<float>::quiet_NaN(), -1.0F);
        float a = std::fmod(angle, 360.0F);
        if (a >= 180.0F) a -= 360.0F;
        if (a < -180.0F) a += 360.0F;
        return a;
    }
    static double wrapDegrees(double angle) {
        if (std::isnan(angle)) return std::numeric_limits<double>::quiet_NaN();
        if (std::isinf(angle)) return std::copysign(std::numeric_limits<double>::quiet_NaN(), -1.0);
        double a = std::fmod(angle, 360.0);
        if (a >= 180.0) a -= 360.0;
        if (a < -180.0) a += 360.0;
        return a;
    }
};
}
