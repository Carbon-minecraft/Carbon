#include "carbon/PerlinNoise.hpp"
#include "carbon/RandomSources.hpp"
#include <bit>
#include <cstdint>
#include <iomanip>
#include <iostream>

int main() {
    char mode;
    std::int64_t seed;
    int farlands;
    double x,y,z;
    while (std::cin >> mode >> seed >> farlands >> x >> y >> z) {
        carbon::SingleThreadedRandomSource rng(seed);
        carbon::PerlinNoise one(rng,farlands!=0);
        float value;
        if (mode=='s') {
            value=one.get(x,y,z);
        } else if (mode=='2') {
            value=one.get(x,y);
        } else {
            carbon::PerlinNoise two(rng,farlands!=0), three(rng,farlands!=0);
            auto stack=carbon::NoiseStack::builder().add(one,0.5,0.75F).add(two,1.0,-0.25F)
                .add(three,2.0,0.125F).build();
            value=stack.get(x,y,z);
        }
        const std::uint32_t bits=std::bit_cast<std::uint32_t>(value);
        const int after=rng.nextInt(1024);
        std::cout << std::hex << std::setw(8) << std::setfill('0') << bits << ' '
                  << std::dec << after << '\n';
    }
}
