#pragma once
#include "carbon/LegacyRandomSource.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace carbon {
// Direct translations of SingleThreadedRandomSource and BitRandomSource.
// This type deliberately has no synchronization, matching the Java source.
class SingleThreadedRandomSource {
public:
    explicit SingleThreadedRandomSource(std::int64_t seed) { setSeed(seed); }
    void setSeed(std::int64_t seed) {
        seed_ = (static_cast<std::uint64_t>(seed) ^ 25214903917ULL) & ((1ULL << 48) - 1);
        hasGaussian_ = false;
    }
    std::int32_t next(int bits) {
        if (bits < 1 || bits > 32) throw std::invalid_argument("bits must be in [1,32]");
        seed_ = (seed_ * 25214903917ULL + 11) & ((1ULL << 48) - 1);
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(seed_ >> (48 - bits)));
    }
    std::int32_t nextInt() { return next(32); }
    std::int32_t nextInt(std::int32_t bound) {
        if (bound <= 0) throw std::invalid_argument("Bound must be positive");
        if ((bound & (bound - 1)) == 0)
            return static_cast<std::int32_t>((static_cast<std::int64_t>(bound) * next(31)) >> 31);
        std::int32_t sample, modulo;
        do {
            sample = next(31); modulo = sample % bound;
        } while (std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(sample - modulo)
                    + static_cast<std::uint32_t>(bound - 1)) < 0);
        return modulo;
    }
    std::int64_t nextLong() {
        const auto upper = static_cast<std::uint64_t>(static_cast<std::int64_t>(next(32)));
        const auto lower = static_cast<std::uint64_t>(static_cast<std::int64_t>(next(32)));
        return std::bit_cast<std::int64_t>((upper << 32) + lower);
    }
    bool nextBoolean() { return next(1) != 0; }
    float nextFloat() { return static_cast<float>(next(24)) * 5.9604645E-8F; }
    double nextDouble() {
        const auto upper = static_cast<std::uint64_t>(next(26));
        const auto lower = static_cast<std::uint64_t>(next(27));
        // The supplied 26.3 jar uses long -> double, then double * 2^-53.
        return static_cast<double>((upper << 27) + lower) * 1.1102230246251565e-16;
    }
    double nextGaussian() {
        if (hasGaussian_) { hasGaussian_ = false; return cachedGaussian_; }
        double x, y, radius;
        do {
            x = 2.0 * nextDouble() - 1.0;
            y = 2.0 * nextDouble() - 1.0;
            radius = x * x + y * y;
        } while (radius >= 1.0 || radius == 0.0);
        const auto multiplier = std::sqrt(-2.0 * std::log(radius) / radius);
        cachedGaussian_ = y * multiplier; hasGaussian_ = true;
        return x * multiplier;
    }
    SingleThreadedRandomSource fork() { return SingleThreadedRandomSource(nextLong()); }
    LegacyPositionalRandomFactory forkPositional() { return LegacyPositionalRandomFactory(nextLong()); }
    void consumeCount(int rounds) { for (int i = 0; i < rounds; ++i) nextInt(); }
private:
    std::uint64_t seed_ = 0;
    double cachedGaussian_ = 0;
    bool hasGaussian_ = false;
};

// Arithmetic subset of RandomSupport.java. No unique-seed or MD5 hash service yet.
struct Seed128bit {
    std::uint64_t seedLo;
    std::uint64_t seedHi;
    Seed128bit xorWith(Seed128bit other) const { return {seedLo ^ other.seedLo, seedHi ^ other.seedHi}; }
};
inline std::uint64_t mixStafford13(std::uint64_t z) {
    z = (z ^ (z >> 30)) * 13787848793156543929ULL;
    z = (z ^ (z >> 27)) * 10723151780598845931ULL;
    return z ^ (z >> 31);
}
inline Seed128bit upgradeSeedTo128bitUnmixed(std::int64_t legacySeed) {
    const auto low = static_cast<std::uint64_t>(legacySeed) ^ 7640891576956012809ULL;
    return {low, low + 11400714819323198485ULL};
}
inline Seed128bit upgradeSeedTo128bit(std::int64_t legacySeed) {
    auto seed = upgradeSeedTo128bitUnmixed(legacySeed);
    return {mixStafford13(seed.seedLo), mixStafford13(seed.seedHi)};
}

// Xoroshiro128PlusPlus.java generator arithmetic. Serialization CODEC is pending.
class Xoroshiro128PlusPlus {
public:
    explicit Xoroshiro128PlusPlus(Seed128bit seed) : seed_(seed) {
        if ((seed_.seedLo | seed_.seedHi) == 0)
            seed_ = {11400714819323198485ULL, 7640891576956012809ULL};
    }
    Xoroshiro128PlusPlus(std::int64_t low, std::int64_t high)
        : Xoroshiro128PlusPlus(Seed128bit{static_cast<std::uint64_t>(low), static_cast<std::uint64_t>(high)}) {}
    std::int64_t nextLong() {
        const auto s0 = seed_.seedLo;
        auto s1 = seed_.seedHi;
        const auto result = std::rotl(s0 + s1, 17) + s0;
        s1 ^= s0;
        seed_.seedLo = std::rotl(s0, 49) ^ s1 ^ (s1 << 21);
        seed_.seedHi = std::rotl(s1, 28);
        return std::bit_cast<std::int64_t>(result);
    }
private:
    Seed128bit seed_;
};
}
