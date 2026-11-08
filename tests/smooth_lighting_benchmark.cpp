#include "carbon/SmoothLighting.hpp"
#include "carbon/FenceBlock.hpp"
#include "carbon/SlabBlock.hpp"
#include "carbon/StairsBlock.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace carbon;

namespace {
VoxelShape sourceLightShape(const BlockInstance& b) {
    switch (b.block) {
    case BlockId::Bedrock: case BlockId::Dirt: case BlockId::GrassBlock:
    case BlockId::Stone: return Shapes::block();
    case BlockId::StoneSlab: return slabVoxelShape(b.slabType);
    case BlockId::StoneStairs: return stairsVoxelShape(b.stairFacing,b.stairHalf,b.stairShape);
    case BlockId::OakFence: return fenceOutlineShape(b.fenceConnections);
    case BlockId::Air: case BlockId::Water: return Shapes::empty();
    }
    return Shapes::empty();
}

bool occupied(const VoxelShape& shape,double x,double y,double z) {
    bool result=false;
    shape.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1) {
        if(x>=x0-1e-6&&x<=x1+1e-6&&y>=y0-1e-6&&y<=y1+1e-6&&z>=z0-1e-6&&z<=z1+1e-6) result=true;
    });
    return result;
}
}

int main() {
    // A bounded stair-heavy fixture magnifies the old repeated shape-compose
    // cost without turning this benchmark into a multi-minute world scan.
    std::vector<BlockInstance> scene;
    constexpr int blockCount=128, probeCount=4096;
    scene.reserve(blockCount);
    constexpr int uniqueStairStates=40;
    for(int i=0;i<blockCount;++i)
        scene.push_back({{i,0,0},BlockId::StoneStairs,SlabType::Bottom,
            static_cast<Direction>(2+((i/10)%4)),static_cast<StairHalf>((i/5)%2),
            static_cast<StairsShape>(i%5)});
    std::vector<std::array<double,3>> probes;
    probes.reserve(probeCount);
    for(int i=0;i<probeCount;++i)
        probes.push_back({(i%blockCount)+0.25,(i%3)*0.25,0.25});

    std::vector<bool> reference;
    reference.reserve(probeCount);
    std::uint64_t baselineShapeBuilds=0;
    const auto before=std::chrono::steady_clock::now();
    for(int i=0;i<probeCount;++i) {
        const auto& block=scene[static_cast<std::size_t>(i%blockCount)];
        const auto shape=sourceLightShape(block); ++baselineShapeBuilds;
        reference.push_back(occupied(shape,probes[static_cast<std::size_t>(i)][0]-block.pos.x,
            probes[static_cast<std::size_t>(i)][1],probes[static_cast<std::size_t>(i)][2]));
    }
    const auto baselineEnd=std::chrono::steady_clock::now();

    const auto cacheStart=std::chrono::steady_clock::now();
    const SmoothLightNeighborhood cached(scene);
    const auto cacheEnd=std::chrono::steady_clock::now();
    const auto warmStart=std::chrono::steady_clock::now();
    const SmoothLightNeighborhood warmCached(scene);
    const auto warmEnd=std::chrono::steady_clock::now();
    std::uint64_t cachedProbeCount=0;
    for(int i=0;i<probeCount;++i) {
        const auto& block=scene[static_cast<std::size_t>(i%blockCount)];
        const BlockPos p=block.pos;
        const bool actual=cached.occludesLightAt(p,probes[static_cast<std::size_t>(i)][0]-p.x,
            probes[static_cast<std::size_t>(i)][1],probes[static_cast<std::size_t>(i)][2]);
        assert(actual==reference[static_cast<std::size_t>(i)]);
        ++cachedProbeCount;
    }
    const auto cachedEnd=std::chrono::steady_clock::now();
    const auto baselineUs=std::chrono::duration_cast<std::chrono::microseconds>(baselineEnd-before).count();
    const auto buildUs=std::chrono::duration_cast<std::chrono::microseconds>(cacheEnd-cacheStart).count();
    const auto queryUs=std::chrono::duration_cast<std::chrono::microseconds>(cachedEnd-cacheEnd).count();
    std::cout << "scene_blocks=" << scene.size() << " probes=" << probeCount
              << " baseline_shape_builds=" << baselineShapeBuilds
              << " unique_shape_keys=" << uniqueStairStates
              << " first_use_shape_builds=" << uniqueStairStates
              << " warm_shape_builds=0"
              << " cached_probes=" << cachedProbeCount
              << " baseline_query_us=" << baselineUs
              << " cache_build_us=" << buildUs
              << " warm_cache_build_us=" << std::chrono::duration_cast<std::chrono::microseconds>(warmEnd-warmStart).count()
              << " cached_query_us=" << queryUs << '\n';
}
