#pragma once

// Source-based Java Edition 26.3 NoiseBasedAquifer scalar block selection.
#include "carbon/XoroshiroWorldgenNoise.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <tuple>

namespace carbon {

enum class AquiferMaterial { KeepDefault, Air, Water, Lava };

struct AquiferFluidStatus {
    int fluidLevel{};
    AquiferMaterial fluidType{AquiferMaterial::Air};
    AquiferMaterial at(int y) const { return y < fluidLevel ? fluidType : AquiferMaterial::Air; }
    friend bool operator==(const AquiferFluidStatus&, const AquiferFluidStatus&) = default;
};

struct AquiferBounds {
    // Half-open source DensityVolume bounds: min plus size in each axis.
    int minX{}, minY{}, minZ{};
    int sizeX{}, sizeY{}, sizeZ{};
};

struct AquiferInputs {
    std::function<float(int,int,int)> barrier;
    std::function<float(int,int,int)> floodedness;
    std::function<float(int,int,int)> spread;
    std::function<float(int,int,int)> lava;
    std::function<float(int,int,int)> exclusion;
    std::function<float(int,int,int)> surfaceLevel;
    std::function<AquiferFluidStatus(int,int,int)> globalFluidPicker;
};

AquiferFluidStatus overworldAquiferFluid(int x, int y, int z, int seaLevel = 63,
                                          bool debugDisableFluidGeneration = false);

class AquiferTerrain {
public:
    AquiferTerrain(AquiferBounds bounds, XoroshiroPositionalRandomFactory positionalRandom,
                   AquiferInputs inputs, bool debugDisableFluidGeneration = false);

    // KeepDefault represents the source's nullable result; callers map it to
    // their configured default stone block. Air is an explicit source result.
    AquiferMaterial computeSubstance(int blockX, int blockY, int blockZ, double density);
    bool shouldScheduleFluidUpdate() const noexcept { return shouldScheduleFluidUpdate_; }

private:
    using Grid = std::tuple<int,int,int>;
    struct Location { int x, y, z; };
    AquiferBounds bounds_;
    XoroshiroPositionalRandomFactory positionalRandom_;
    AquiferInputs inputs_;
    bool debugDisableFluidGeneration_{};
    bool shouldScheduleFluidUpdate_{};
    int minGridX_{}, minGridY_{}, minGridZ_{}, maxGridX_{}, maxGridY_{}, maxGridZ_{};
    int skipSamplingAboveY_{};
    std::map<Grid, Location> locations_;
    std::map<Grid, AquiferFluidStatus> statuses_;
    std::map<std::pair<int,int>, int> surfaceCache_;

    static int gridX(int block) { return block >> 4; }
    static int gridY(int block);
    static int gridZ(int block) { return block >> 4; }
    static int fromGridX(int grid, int offset) { return (grid << 4) + offset; }
    static int fromGridY(int grid, int offset) { return grid * 12 + offset; }
    static int fromGridZ(int grid, int offset) { return (grid << 4) + offset; }
    static double similarity(int first, int second);
    static int floorDiv(int value, int divisor);
    int surfaceLevel(int x, int z);
    int maxSurfaceLevel(int minX, int minZ, int maxX, int maxZ);
    Location aquiferLocation(int gridX, int gridY, int gridZ);
    AquiferFluidStatus aquiferStatus(const Grid& grid);
    AquiferFluidStatus computeFluid(int x, int y, int z);
    int adjustSurfaceLevel(int surface) const { return surface + 8; }
    int computeSurfaceLevel(int x, int y, int z, AquiferFluidStatus globalFluid,
                            int lowestSurface, bool surfaceUnderGlobalFluid);
    int randomizedFluidSurfaceLevel(int x, int y, int z, int lowestSurface);
    AquiferMaterial computeFluidType(int x, int y, int z, AquiferFluidStatus globalFluid, int fluidSurface);
    double calculatePressure(int x, int y, int z, AquiferFluidStatus first,
                             AquiferFluidStatus second, double& barrierCache);
};

} // namespace carbon
