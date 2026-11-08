#include "carbon/Noise.hpp"
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    std::string line;
    while(std::getline(std::cin,line)) {
        std::istringstream in(line);
        char mode; std::int64_t seed; int discard; double x,y,z;
        if(!(in>>mode>>seed>>discard>>x>>y>>z)) return 2;
        carbon::SingleThreadedRandomSource rng(seed);
        carbon::SimplexNoise noise(rng,discard!=0);
        if(mode=='w') {
            std::cout << std::hex << std::setw(16) << std::setfill('0') << std::bit_cast<std::uint64_t>(carbon::GradientNoise::wrap(x,discard!=0)) << '\n';
            continue;
        }
        const float value=mode=='2' ? noise.get(x,y) : noise.get(x,y,z);
        std::cout << std::hex << std::setw(8) << std::setfill('0') << std::bit_cast<std::uint32_t>(value) << '\n';
    }
}
