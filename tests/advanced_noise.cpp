#include "carbon/AdvancedNoise.hpp"
#include <bit>
#include <cstdint>
#include <iomanip>
#include <iostream>

int main() {
    std::int64_t seed;
    int mode, baseOctave, count;
    double baseAmplitude, x, y, z;
    while (std::cin >> seed >> mode >> baseOctave >> count >> baseAmplitude >> x >> y >> z) {
        carbon::NormalNoise::Builder builder;
        builder.setBaseOctave(baseOctave).setOctaveCount(count).setBaseAmplitude(baseAmplitude);
        if (mode == 0) builder.setNormalize(false);
        else if (mode == 2) builder.setLegacyNormalization();
        for (int i = 0; i < count; ++i) {
            double modifier;
            std::cin >> modifier;
            if (modifier != 1.0) builder.setAmplitudeModifier(i, modifier);
        }
        auto normal = builder.build();
        carbon::LegacyRandomSource random(seed);
        const float value = normal.create(random).get(x, y, z);
        const auto bits = std::bit_cast<std::uint32_t>(value);
        std::cout << std::hex << std::setw(8) << std::setfill('0') << bits << ' '
                  << std::dec << random.nextInt(1024) << '\n';
    }
}
