#pragma once

#include "carbon/ClientRenderer.hpp"
#include "carbon/VoxelShapes.hpp"

#include <array>
#include <span>
#include <unordered_map>

namespace carbon {

// Source-traced smooth lighting subset for the native world renderer. The
// lookup is the current rendered block neighborhood; missing positions are
// air. Sky light uses direct-sky visibility for this flat-world client slice.
struct SmoothLightSample {
    std::array<float, 4> vertexBrightness{1, 1, 1, 1};
    std::array<std::uint8_t, 4> skyLight{15, 15, 15, 15};
    std::array<std::uint8_t, 4> blockLight{};
};

class SmoothLightNeighborhood {
public:
    explicit SmoothLightNeighborhood(std::span<const BlockInstance> blocks);
    const BlockInstance* find(const BlockPos& pos) const;
    bool directSkyVisible(const BlockPos& pos) const;
    bool occludesLightAt(const BlockPos& pos, double x, double y, double z) const;
private:
    struct CachedBlock {
        BlockInstance instance;
        const VoxelShape* lightShape{};
        bool lightOccluder{};
    };
    std::unordered_map<std::int64_t, CachedBlock> blocks_;
    std::unordered_map<std::int64_t, std::int32_t> topOpaqueByColumn_;
};

SmoothLightSample sampleSmoothLighting(const SourceModelQuad& quad,
                                       const BlockInstance& block,
                                       const SmoothLightNeighborhood& neighborhood);

} // namespace carbon
