#include "carbon/AquiferTerrain.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace carbon {
namespace {
constexpr int kBelowMinY = -32512; // DimensionType.WAY_BELOW_MIN_Y in supplied source (MIN_Y=-2032).
constexpr int kMinYTimesTwo = -4064; // DimensionType.MIN_Y * 2, used by NoiseBasedChunkGenerator's empty picker status.
constexpr double kFlowingUpdateSimilarity = 1.0 - (144.0 - 100.0) / 25.0;
constexpr int kSurfaceOffsets[13][2] = {{0,0},{-2,-1},{-1,-1},{0,-1},{1,-1},{-3,0},{-2,0},{-1,0},{1,0},{-2,1},{-1,1},{0,1},{1,1}};
int javaFloor(double value) { return Mth::floor(value); }
}

AquiferFluidStatus overworldAquiferFluid(int, int y, int, int seaLevel, bool debugDisableFluidGeneration) {
    if (debugDisableFluidGeneration) return {kMinYTimesTwo, AquiferMaterial::Air};
    if (y < std::min(-54, seaLevel)) return {-54, AquiferMaterial::Lava};
    return {seaLevel, AquiferMaterial::Water};
}

int AquiferTerrain::floorDiv(int value, int divisor) {
    int q = value / divisor;
    const int r = value % divisor;
    if (r != 0 && ((r ^ divisor) < 0)) --q;
    return q;
}
int AquiferTerrain::gridY(int block) { return floorDiv(block, 12); }
double AquiferTerrain::similarity(int first, int second) { return 1.0 - static_cast<double>(second - first) / 25.0; }

AquiferTerrain::AquiferTerrain(AquiferBounds bounds, XoroshiroPositionalRandomFactory positionalRandom,
                               AquiferInputs inputs, bool debugDisableFluidGeneration)
    : bounds_(bounds), positionalRandom_(std::move(positionalRandom)), inputs_(std::move(inputs)),
      debugDisableFluidGeneration_(debugDisableFluidGeneration) {
    if (bounds.sizeX <= 0 || bounds.sizeY <= 0 || bounds.sizeZ <= 0) throw std::invalid_argument("aquifer bounds must have positive size");
    if (!inputs_.barrier || !inputs_.floodedness || !inputs_.spread || !inputs_.lava || !inputs_.exclusion ||
        !inputs_.surfaceLevel || !inputs_.globalFluidPicker) throw std::invalid_argument("aquifer callbacks are required");
    minGridX_ = gridX(bounds.minX - 5);
    maxGridX_ = gridX(bounds.minX + bounds.sizeX - 1 - 5) + 1;
    minGridY_ = gridY(bounds.minY + 1) - 1;
    maxGridY_ = gridY(bounds.minY + bounds.sizeY - 1 + 1) + 1;
    minGridZ_ = gridZ(bounds.minZ - 5);
    maxGridZ_ = gridZ(bounds.minZ + bounds.sizeZ - 1 - 5) + 1;
    const int maxAdjustedSurface = adjustSurfaceLevel(maxSurfaceLevel(fromGridX(minGridX_, 0), fromGridZ(minGridZ_, 0),
                                                                         fromGridX(maxGridX_, 9), fromGridZ(maxGridZ_, 9)));
    const int skipSamplingAboveGridY = gridY(maxAdjustedSurface + 12) + 1;
    skipSamplingAboveY_ = fromGridY(skipSamplingAboveGridY, 11) - 1;
}

int AquiferTerrain::surfaceLevel(int x, int z) {
    const int quantizedX = floorDiv(x, 4) * 4;
    const int quantizedZ = floorDiv(z, 4) * 4;
    const auto key = std::pair{quantizedX, quantizedZ};
    auto [it, inserted] = surfaceCache_.try_emplace(key, 0);
    if (inserted) it->second = javaFloor(inputs_.surfaceLevel(quantizedX, 0, quantizedZ));
    return it->second;
}

int AquiferTerrain::maxSurfaceLevel(int minX, int minZ, int maxX, int maxZ) {
    const int minQuartX = floorDiv(minX, 4), maxQuartX = floorDiv(maxX, 4);
    const int minQuartZ = floorDiv(minZ, 4), maxQuartZ = floorDiv(maxZ, 4);
    int maxY = std::numeric_limits<int>::min();
    for (int qz = minQuartZ; qz <= maxQuartZ; ++qz) {
        for (int qx = minQuartX; qx <= maxQuartX; ++qx) {
            const int x = qx * 4, z = qz * 4;
            maxY = std::max(maxY, surfaceLevel(x, z));
        }
    }
    return maxY;
}

AquiferTerrain::Location AquiferTerrain::aquiferLocation(int gx, int gy, int gz) {
    const Grid key{gx, gy, gz};
    if (auto it = locations_.find(key); it != locations_.end()) return it->second;
    auto random = positionalRandom_.at(gx, gy, gz);
    Location location{fromGridX(gx, random.nextInt(10)), fromGridY(gy, random.nextInt(9)), fromGridZ(gz, random.nextInt(10))};
    locations_.emplace(key, location);
    return location;
}

AquiferFluidStatus AquiferTerrain::aquiferStatus(const Grid& key) {
    if (auto it = statuses_.find(key); it != statuses_.end()) return it->second;
    const auto [x, y, z] = aquiferLocation(std::get<0>(key), std::get<1>(key), std::get<2>(key));
    auto status = computeFluid(x, y, z);
    statuses_.emplace(key, status);
    return status;
}

AquiferMaterial AquiferTerrain::computeSubstance(int blockX, int blockY, int blockZ, double density) {
    if (density > 0.0) { shouldScheduleFluidUpdate_ = false; return AquiferMaterial::KeepDefault; }
    const AquiferFluidStatus globalFluid = inputs_.globalFluidPicker(blockX, blockY, blockZ);
    if (blockY > skipSamplingAboveY_) { shouldScheduleFluidUpdate_ = false; return globalFluid.at(blockY); }
    if (globalFluid.at(blockY) == AquiferMaterial::Lava) {
        shouldScheduleFluidUpdate_ = false;
        return debugDisableFluidGeneration_ ? AquiferMaterial::Air : AquiferMaterial::Lava;
    }

    const int xAnchor = gridX(blockX - 5), yAnchor = gridY(blockY + 1), zAnchor = gridZ(blockZ - 5);
    int d1 = INT_MAX, d2 = INT_MAX, d3 = INT_MAX, d4 = INT_MAX;
    Grid c1{}, c2{}, c3{}, c4{};
    for (int xOffset = 0; xOffset <= 1; ++xOffset) {
        for (int yOffset = -1; yOffset <= 1; ++yOffset) {
            for (int zOffset = 0; zOffset <= 1; ++zOffset) {
                Grid candidate{xAnchor + xOffset, yAnchor + yOffset, zAnchor + zOffset};
                const auto location = aquiferLocation(std::get<0>(candidate), std::get<1>(candidate), std::get<2>(candidate));
                const int dx = location.x - blockX, dy = location.y - blockY, dz = location.z - blockZ;
                const int distance = dx * dx + dy * dy + dz * dz;
                if (d1 >= distance) { d4=d3; c4=c3; d3=d2; c3=c2; d2=d1; c2=c1; d1=distance; c1=candidate; }
                else if (d2 >= distance) { d4=d3; c4=c3; d3=d2; c3=c2; d2=distance; c2=candidate; }
                else if (d3 >= distance) { d4=d3; c4=c3; d3=distance; c3=candidate; }
                else if (d4 >= distance) { d4=distance; c4=candidate; }
            }
        }
    }

    const auto status1 = aquiferStatus(c1);
    const double sim12 = similarity(d1, d2);
    const AquiferMaterial fluid = status1.at(blockY);
    const AquiferMaterial actualFluid = debugDisableFluidGeneration_ ? AquiferMaterial::Air : fluid;
    if (sim12 <= 0.0) {
        if (sim12 >= kFlowingUpdateSimilarity) shouldScheduleFluidUpdate_ = !(status1 == aquiferStatus(c2));
        else shouldScheduleFluidUpdate_ = false;
        return actualFluid;
    }
    if (fluid == AquiferMaterial::Water && inputs_.globalFluidPicker(blockX, blockY - 1, blockZ).at(blockY - 1) == AquiferMaterial::Lava) {
        shouldScheduleFluidUpdate_ = true;
        return actualFluid;
    }
    double barrierValue = std::numeric_limits<double>::quiet_NaN();
    const auto status2 = aquiferStatus(c2);
    const double barrier12 = sim12 * calculatePressure(blockX, blockY, blockZ, status1, status2, barrierValue);
    if (density + barrier12 > 0.0) { shouldScheduleFluidUpdate_ = false; return AquiferMaterial::KeepDefault; }
    const auto status3 = aquiferStatus(c3);
    const double sim13 = similarity(d1, d3);
    if (sim13 > 0.0) {
        const double barrier13 = sim12 * sim13 * calculatePressure(blockX, blockY, blockZ, status1, status3, barrierValue);
        if (density + barrier13 > 0.0) { shouldScheduleFluidUpdate_ = false; return AquiferMaterial::KeepDefault; }
    }
    const double sim23 = similarity(d2, d3);
    if (sim23 > 0.0) {
        const double barrier23 = sim12 * sim23 * calculatePressure(blockX, blockY, blockZ, status2, status3, barrierValue);
        if (density + barrier23 > 0.0) { shouldScheduleFluidUpdate_ = false; return AquiferMaterial::KeepDefault; }
    }
    const bool mayFlow12 = !(status1 == status2);
    const bool mayFlow23 = sim23 >= kFlowingUpdateSimilarity && !(status2 == status3);
    const bool mayFlow13 = sim13 >= kFlowingUpdateSimilarity && !(status1 == status3);
    if (!mayFlow12 && !mayFlow23 && !mayFlow13) {
        shouldScheduleFluidUpdate_ = sim13 >= kFlowingUpdateSimilarity &&
            similarity(d1, d4) >= kFlowingUpdateSimilarity && !(status1 == aquiferStatus(c4));
    } else shouldScheduleFluidUpdate_ = true;
    return actualFluid;
}

AquiferFluidStatus AquiferTerrain::computeFluid(int x, int y, int z) {
    const auto globalFluid = inputs_.globalFluidPicker(x, y, z);
    int lowestSurface = INT_MAX;
    const int top = y + 12, bottom = y - 12;
    bool surfaceUnderGlobalFluid = false;
    for (const auto& offset : kSurfaceOffsets) {
        const int sampleX = x + offset[0] * 16, sampleZ = z + offset[1] * 16;
        const int surface = surfaceLevel(sampleX, sampleZ);
        const int adjusted = adjustSurfaceLevel(surface);
        const bool center = offset[0] == 0 && offset[1] == 0;
        if (center && bottom > adjusted) return globalFluid;
        const bool topPokesAboveSurface = top > adjusted;
        if (topPokesAboveSurface || center) {
            const auto globalAtSurface = inputs_.globalFluidPicker(sampleX, adjusted, sampleZ);
            if (globalAtSurface.at(adjusted) != AquiferMaterial::Air) {
                if (center) surfaceUnderGlobalFluid = true;
                if (topPokesAboveSurface) return globalAtSurface;
            }
        }
        lowestSurface = std::min(lowestSurface, surface);
    }
    const int fluidSurface = computeSurfaceLevel(x, y, z, globalFluid, lowestSurface, surfaceUnderGlobalFluid);
    return {fluidSurface, computeFluidType(x, y, z, globalFluid, fluidSurface)};
}

int AquiferTerrain::computeSurfaceLevel(int x, int y, int z, AquiferFluidStatus globalFluid, int lowestSurface, bool surfaceUnderGlobalFluid) {
    double partiallyFloodedness, fullyFloodedness;
    if (inputs_.exclusion(x, y, z) > 0.0F) {
        partiallyFloodedness = -1.0; fullyFloodedness = -1.0;
    } else {
        const int distanceBelowSurface = adjustSurfaceLevel(lowestSurface) - y;
        const double floodednessFactor = surfaceUnderGlobalFluid
            ? Mth::clampedLerp((distanceBelowSurface - 0.0) / (64.0 - 0.0), 1.0, 0.0) : 0.0;
        const double floodednessNoise = Mth::clamp(static_cast<double>(inputs_.floodedness(x,y,z)), -1.0, 1.0);
        const double mapFactor = Mth::inverseLerp(floodednessFactor, 1.0, 0.0);
        const double fullyThreshold = Mth::lerp(mapFactor, -0.3, 0.8);
        const double partiallyThreshold = Mth::lerp(mapFactor, -0.8, 0.4);
        partiallyFloodedness = floodednessNoise - partiallyThreshold;
        fullyFloodedness = floodednessNoise - fullyThreshold;
    }
    if (fullyFloodedness > 0.0) return globalFluid.fluidLevel;
    if (partiallyFloodedness > 0.0) return randomizedFluidSurfaceLevel(x,y,z,lowestSurface);
    return kBelowMinY;
}

int AquiferTerrain::randomizedFluidSurfaceLevel(int x, int y, int z, int lowestSurface) {
    const int cellX = floorDiv(x,16), cellY = floorDiv(y,40), cellZ = floorDiv(z,16);
    const int cellMiddleY = cellY * 40 + 20;
    const float spread = inputs_.spread(cellX,cellY,cellZ) * 10.0F;
    const int quantized = Mth::floor(static_cast<double>(spread) / 3.0) * 3;
    const int target = cellMiddleY + quantized;
    return std::min(lowestSurface, target);
}

AquiferMaterial AquiferTerrain::computeFluidType(int x, int y, int z, AquiferFluidStatus globalFluid, int fluidSurface) {
    auto type = globalFluid.fluidType;
    if (fluidSurface <= -10 && fluidSurface != kBelowMinY && globalFluid.fluidType != AquiferMaterial::Lava) {
        const int cellX=floorDiv(x,64), cellY=floorDiv(y,40), cellZ=floorDiv(z,64);
        const float lava = inputs_.lava(cellX,cellY,cellZ);
        if (std::fabs(lava) > 0.3F) type = AquiferMaterial::Lava;
    }
    return type;
}

double AquiferTerrain::calculatePressure(int x, int y, int z, AquiferFluidStatus first,
                                         AquiferFluidStatus second, double& barrierCache) {
    const auto type1 = first.at(y), type2 = second.at(y);
    if ((type1 == AquiferMaterial::Lava && type2 == AquiferMaterial::Water) ||
        (type1 == AquiferMaterial::Water && type2 == AquiferMaterial::Lava)) return 2.0;
    const int fluidYDiff = std::abs(first.fluidLevel - second.fluidLevel);
    if (fluidYDiff == 0) return 0.0;
    const double averageFluidY = 0.5 * (first.fluidLevel + second.fluidLevel);
    const double aboveAverage = y + 0.5 - averageFluidY;
    const double base = fluidYDiff / 2.0;
    const double towardMiddle = base - std::abs(aboveAverage);
    double gradient;
    if (aboveAverage > 0.0) gradient = towardMiddle > 0.0 ? towardMiddle / 1.5 : towardMiddle / 2.5;
    else { const double center = 3.0 + towardMiddle; gradient = center > 0.0 ? center / 3.0 : center / 10.0; }
    double noise;
    if (gradient < -2.0 || gradient > 2.0) noise = 0.0;
    else {
        if (std::isnan(barrierCache)) barrierCache = inputs_.barrier(x,y,z);
        noise = barrierCache;
    }
    return 2.0 * (noise + gradient);
}

} // namespace carbon
