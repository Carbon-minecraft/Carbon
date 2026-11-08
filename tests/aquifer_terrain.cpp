#include "carbon/AquiferTerrain.hpp"

#include <cassert>

int main() {
    carbon::XoroshiroPositionalRandomFactory random({0x123456789abcdef0ULL, 0xfedcba9876543210ULL});
    carbon::AquiferInputs inputs;
    inputs.barrier = [](int,int,int) { return 0.0F; };
    inputs.floodedness = [](int,int,int) { return 1.0F; };
    inputs.spread = [](int,int,int) { return 0.0F; };
    inputs.lava = [](int,int,int) { return 0.0F; };
    inputs.exclusion = [](int,int,int) { return 0.0F; };
    inputs.surfaceLevel = [](int,int,int) { return 64.0F; };
    inputs.globalFluidPicker = [](int x,int y,int z) { return carbon::overworldAquiferFluid(x,y,z); };

    carbon::AquiferTerrain aquifer({0,-64,0,16,384,16}, random, inputs);
    assert(aquifer.computeSubstance(0,-10,0,0.25) == carbon::AquiferMaterial::KeepDefault);
    assert(!aquifer.shouldScheduleFluidUpdate());
    assert(aquifer.computeSubstance(0,-10,0,-0.25) == carbon::AquiferMaterial::Water);
    assert(aquifer.computeSubstance(0,-60,0,-0.25) == carbon::AquiferMaterial::Lava);
    assert(aquifer.computeSubstance(0,63,0,-0.25) == carbon::AquiferMaterial::Air);

    assert(carbon::overworldAquiferFluid(0,-55,0).fluidType == carbon::AquiferMaterial::Lava);
    assert(carbon::overworldAquiferFluid(0,-54,0).fluidType == carbon::AquiferMaterial::Water);
    assert(carbon::overworldAquiferFluid(0,63,0).at(63) == carbon::AquiferMaterial::Air);
    assert(carbon::overworldAquiferFluid(0,-64,0,63,true).at(-64) == carbon::AquiferMaterial::Air);
}
