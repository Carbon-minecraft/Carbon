#include "carbon/XoroshiroWorldgenNoise.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

int main() {
    const auto abc = carbon::detail::md5("abc");
    constexpr std::array<std::uint8_t,16> expected{{
        0x90,0x01,0x50,0x98,0x3c,0xd2,0x4f,0xb0,0xd6,0x96,0x3f,0x7d,0x28,0xe1,0x7f,0x72
    }};
    assert(abc == expected);

    carbon::XoroshiroRandomSource source(123456789LL);
    const auto first = source.nextLong();
    // Seed 123456789's first Xoroshiro128++ value is also covered by the
    // Java 26.3 RandomSupport/Xoroshiro128PlusPlus parity vectors.
    assert(first == 3219654894476264721LL);
    carbon::XoroshiroRandomSource same(123456789LL);
    assert(first == same.nextLong());
    const auto bounded = source.nextInt(17);
    assert(bounded >= 0 && bounded < 17);
    const auto unit = source.nextDouble();
    assert(unit >= 0.0 && unit < 1.0);

    carbon::XoroshiroRandomSource worldSeed(987654321LL);
    carbon::XoroshiroPositionalRandomFactory positional(worldSeed.forkPositionalSeed());
    auto a = positional.fromHashOf("octave_-9");
    auto b = positional.fromHashOf("octave_-9");
    assert(a.nextLong() == b.nextLong());
    auto c = positional.at(-17, -64, 33);
    auto d = positional.at(-17, -64, 33);
    assert(c.nextLong() == d.nextLong());

    auto parameters = carbon::NormalNoise::builder()
        .setBaseAmplitude(0.8880832896205223)
        .setBaseOctave(-9)
        .setOctaveCount(9)
        .build();
    carbon::XoroshiroRandomSource noiseSeed(42);
    carbon::XoroshiroNormalNoise noise(noiseSeed, parameters);
    assert(std::isfinite(noise.get(-16.0, -64.0, 16.0)));
    std::cout << "Xoroshiro worldgen RNG and normal-noise construction passed\n";
}
