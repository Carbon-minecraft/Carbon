#include "carbon/BlendedTerrainNoise.hpp"

#include <bit>
#include <cstdint>
#include <iomanip>
#include <iostream>

int main() {
    std::int64_t seed; double x, y, z, xzScale, yScale, xzFactor, yFactor, smear;
    while (std::cin >> seed >> x >> y >> z >> xzScale >> yScale >> xzFactor >> yFactor >> smear) {
        carbon::XoroshiroRandomSource initial(seed);
        carbon::XoroshiroPositionalRandomFactory factory(initial.forkPositionalSeed());
        auto random = factory.fromHashOf("minecraft:terrain");
        carbon::BlendedTerrainNoise noise(random, xzScale, yScale, xzFactor, yFactor, smear);
        const float value = noise.sample(x, y, z);
        std::cout << std::hex << std::setw(8) << std::setfill('0') << std::bit_cast<std::uint32_t>(value)
                  << ' ' << std::dec << random.nextInt(1024) << '\n';
    }
}
