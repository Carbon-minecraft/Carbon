#include "carbon/OverworldNoise.hpp"
#include <cassert>
#include <bit>
#include <cstdint>
#include <iostream>

int main() {
    carbon::OverworldNoise a(987654321),b(987654321),c(987654322);
    const auto x=std::bit_cast<std::uint32_t>(a.sample("minecraft:continentalness",-164.0,0.0,276.0));
    const auto y=std::bit_cast<std::uint32_t>(b.sample("minecraft:continentalness",-164.0,0.0,276.0));
    const auto z=std::bit_cast<std::uint32_t>(c.sample("minecraft:continentalness",-164.0,0.0,276.0));
    assert(x==y);
    assert(x!=z);
    std::cout<<"seeded Overworld noise deterministic\n";
}
