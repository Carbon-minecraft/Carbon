#pragma once
#include <atomic>
#include <cstdint>
#include <string_view>

namespace carbon {
// Source: net/minecraft/world/level/levelgen/{LegacyRandomSource,BitRandomSource,
// MarsagliaPolarGaussian}.java. Integer overflow follows Java wraparound.
class LegacyRandomSource {
public:
    explicit LegacyRandomSource(std::int64_t seed) { setSeed(seed); }
    void setSeed(std::int64_t seed);
    std::int32_t next(int bits);
    std::int32_t nextInt() { return next(32); }
    std::int32_t nextInt(std::int32_t bound);
    std::int64_t nextLong();
    bool nextBoolean() { return next(1) != 0; }
    float nextFloat() { return static_cast<float>(next(24)) * 0x1p-24f; }
    double nextDouble();
    double nextGaussian();
    LegacyRandomSource fork() { return LegacyRandomSource(nextLong()); }
    void consumeCount(int rounds) { for (int i = 0; i < rounds; ++i) nextInt(); }
private:
    std::atomic<std::uint64_t> seed_{0};
    double cachedGaussian_ = 0;
    bool hasGaussian_ = false;
};
std::int64_t positionalSeed(std::int32_t x, std::int32_t y, std::int32_t z);
std::int32_t javaStringHash(std::u16string_view value);
class LegacyPositionalRandomFactory {
public:
    explicit LegacyPositionalRandomFactory(std::int64_t seed) : seed_(seed) {}
    LegacyRandomSource at(std::int32_t x, std::int32_t y, std::int32_t z) const;
    LegacyRandomSource fromHashOf(std::u16string_view name) const;
    LegacyRandomSource fromSeed(std::int64_t seed) const { return LegacyRandomSource(seed); }
private:
    std::int64_t seed_;
};
}
