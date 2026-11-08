#include "carbon/LegacyRandomSource.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace carbon {
namespace {
constexpr std::uint64_t mask = (1ULL << 48) - 1;
std::int32_t i32(std::uint32_t v) { return std::bit_cast<std::int32_t>(v); }
std::int64_t i64(std::uint64_t v) { return std::bit_cast<std::int64_t>(v); }
}
void LegacyRandomSource::setSeed(std::int64_t value) {
    auto old = seed_.load();
    if (!seed_.compare_exchange_strong(old, (static_cast<std::uint64_t>(value) ^ 25214903917ULL) & mask))
        throw std::runtime_error("Concurrent access to LegacyRandomSource");
    hasGaussian_ = false;
}
std::int32_t LegacyRandomSource::next(int bits) {
    if (bits < 1 || bits > 32) throw std::invalid_argument("bits must be in [1,32]");
    auto old = seed_.load();
    auto updated = (old * 25214903917ULL + 11) & mask;
    if (!seed_.compare_exchange_strong(old, updated))
        throw std::runtime_error("Concurrent access to LegacyRandomSource");
    return i32(static_cast<std::uint32_t>(updated >> (48 - bits)));
}
std::int32_t LegacyRandomSource::nextInt(std::int32_t bound) {
    if (bound <= 0) throw std::invalid_argument("Bound must be positive");
    if ((bound & (bound - 1)) == 0)
        return static_cast<std::int32_t>((static_cast<std::int64_t>(bound) * next(31)) >> 31);
    std::int32_t sample, modulo;
    do {
        sample = next(31);
        modulo = sample % bound;
    } while (i32(static_cast<std::uint32_t>(sample - modulo) + static_cast<std::uint32_t>(bound - 1)) < 0);
    return modulo;
}
std::int64_t LegacyRandomSource::nextLong() {
    auto upper = static_cast<std::uint64_t>(static_cast<std::int64_t>(next(32)));
    auto lower = static_cast<std::uint64_t>(static_cast<std::int64_t>(next(32)));
    return i64((upper << 32) + lower);
}
double LegacyRandomSource::nextDouble() {
    auto upper = static_cast<std::uint64_t>(next(26));
    auto lower = static_cast<std::uint64_t>(next(27));
    // BitRandomSource.nextDouble in the supplied 26.3 jar converts the
    // 53-bit integer to double and multiplies by the exact 2^-53 double.
    return static_cast<double>((upper << 27) + lower) * 1.1102230246251565e-16;
}
double LegacyRandomSource::nextGaussian() {
    if (hasGaussian_) { hasGaussian_ = false; return cachedGaussian_; }
    double x, y, radius;
    do {
        x = 2.0 * nextDouble() - 1.0;
        y = 2.0 * nextDouble() - 1.0;
        radius = x * x + y * y;
    } while (radius >= 1.0 || radius == 0.0);
    auto multiplier = std::sqrt(-2.0 * std::log(radius) / radius);
    cachedGaussian_ = y * multiplier;
    hasGaussian_ = true;
    return x * multiplier;
}
std::int64_t positionalSeed(std::int32_t x, std::int32_t y, std::int32_t z) {
    auto xTerm = i32(static_cast<std::uint32_t>(x) * 3129871U);
    auto seed = static_cast<std::uint64_t>(static_cast<std::int64_t>(xTerm))
        ^ (static_cast<std::uint64_t>(static_cast<std::int64_t>(z)) * 116129781ULL)
        ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(y));
    seed = seed * seed * 42317861ULL + seed * 11;
    return i64(seed) >> 16;
}
std::int32_t javaStringHash(std::u16string_view value) {
    std::uint32_t hash = 0;
    for (char16_t c : value) hash = hash * 31 + c;
    return i32(hash);
}
LegacyRandomSource LegacyPositionalRandomFactory::at(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return LegacyRandomSource(positionalSeed(x, y, z) ^ seed_);
}
LegacyRandomSource LegacyPositionalRandomFactory::fromHashOf(std::u16string_view name) const {
    return LegacyRandomSource(static_cast<std::int64_t>(javaStringHash(name)) ^ seed_);
}
}
