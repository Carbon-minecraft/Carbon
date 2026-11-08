#include "carbon/SmoothLighting.hpp"

#include "carbon/Coordinates.hpp"
#include "carbon/FenceBlock.hpp"
#include "carbon/SlabBlock.hpp"
#include "carbon/StairsBlock.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <mutex>

namespace carbon {
namespace {

VoxelShape shapeOf(const BlockInstance* block) {
    if (!block) return Shapes::empty();
    switch (block->block) {
    case BlockId::Bedrock:
    case BlockId::Dirt:
    case BlockId::GrassBlock:
    case BlockId::Stone: return Shapes::block();
    case BlockId::StoneSlab: return slabVoxelShape(block->slabType);
    case BlockId::StoneStairs: return stairsVoxelShape(block->stairFacing, block->stairHalf, block->stairShape);
    case BlockId::OakFence: return fenceOutlineShape(block->fenceConnections);
    case BlockId::Air:
    case BlockId::Water: return Shapes::empty();
    }
    return Shapes::empty();
}

std::size_t lightShapeIndex(const BlockInstance& block) {
    switch (block.block) {
    case BlockId::Bedrock: case BlockId::Dirt: case BlockId::GrassBlock: case BlockId::Stone:
        return 0; // same full-block light geometry
    case BlockId::StoneSlab:
        return 1 + static_cast<std::size_t>(block.slabType);
    case BlockId::StoneStairs: {
        const std::size_t facing = block.stairFacing == Direction::East ? 1 :
            block.stairFacing == Direction::South ? 2 : block.stairFacing == Direction::West ? 3 : 0;
        return 4 + facing * 10 + static_cast<std::size_t>(block.stairHalf) * 5 +
            static_cast<std::size_t>(block.stairShape);
    }
    case BlockId::Air: case BlockId::Water: case BlockId::OakFence:
        return 0;
    }
    return 0;
}

const VoxelShape* cachedLightShape(const BlockInstance& block) {
    struct Entry { std::once_flag once; std::unique_ptr<VoxelShape> shape; };
    // Supported immutable light geometry has one full block, three slab states,
    // and 4 facings * 2 halves * 5 stair shapes. A fixed table bounds lifetime
    // and memory while sharing exact state geometry across every scene snapshot.
    static std::array<Entry,44> entries;
    const auto index = lightShapeIndex(block);
    auto& entry = entries[index];
    std::call_once(entry.once, [&] { entry.shape = std::make_unique<VoxelShape>(shapeOf(&block)); });
    return entry.shape.get();
}

bool occupiedAt(const VoxelShape& shape, double x, double y, double z) {
    bool occupied = false;
    shape.forAllBoxes([&](double x0,double y0,double z0,double x1,double y1,double z1) {
        if (x >= x0-1e-6 && x <= x1+1e-6 && y >= y0-1e-6 && y <= y1+1e-6 && z >= z0-1e-6 && z <= z1+1e-6)
            occupied = true;
    });
    return occupied;
}

std::array<int,3> faceStep(const std::array<float,3>& normal) {
    if (normal[1] < -0.5f) return {0,-1,0};
    if (normal[1] > 0.5f) return {0,1,0};
    if (normal[2] < -0.5f) return {0,0,-1};
    if (normal[2] > 0.5f) return {0,0,1};
    if (normal[0] < -0.5f) return {-1,0,0};
    return {1,0,0};
}

float cardinalShade(const std::array<int,3>& n) {
    if (n[1] < 0) return 0.5f;
    if (n[1] > 0) return 1.0f;
    if (n[0] != 0) return 0.6f;
    return 0.8f;
}

} // namespace

SmoothLightNeighborhood::SmoothLightNeighborhood(std::span<const BlockInstance> blocks) {
    blocks_.reserve(blocks.size());
    topOpaqueByColumn_.reserve(blocks.size());
    for (const auto& block : blocks) {
        // Shape composition for stairs/fences joins voxel grids and can be
        // substantially more expensive than checking a cached box list. Build
        // it once per scene snapshot instead of once for every AO probe.
        const bool occluder = block.block != BlockId::Air && block.block != BlockId::Water &&
                              block.block != BlockId::OakFence;
        CachedBlock cached{block, occluder ? cachedLightShape(block) : nullptr, occluder};
        blocks_.insert_or_assign(block.pos.asLong(), std::move(cached));
        if (block.block==BlockId::Air || block.block==BlockId::Water || block.block==BlockId::OakFence) continue;
        const auto key=BlockPos::asLong(block.pos.x,0,block.pos.z);
        auto [it,inserted]=topOpaqueByColumn_.try_emplace(key,block.pos.y);
        if (!inserted) it->second=std::max(it->second,block.pos.y);
    }
}

const BlockInstance* SmoothLightNeighborhood::find(const BlockPos& pos) const {
    const auto it=blocks_.find(pos.asLong());
    return it==blocks_.end()?nullptr:&it->second.instance;
}

bool SmoothLightNeighborhood::occludesLightAt(const BlockPos& pos, double x, double y, double z) const {
    const auto it = blocks_.find(pos.asLong());
    // Fence collision geometry is open to light in the 26.3 block properties;
    // its visible posts/rails still receive AO from adjacent opaque shapes.
    return it != blocks_.end() && it->second.lightOccluder && it->second.lightShape &&
           occupiedAt(*it->second.lightShape, x, y, z);
}

bool SmoothLightNeighborhood::directSkyVisible(const BlockPos& pos) const {
    const auto it=topOpaqueByColumn_.find(BlockPos::asLong(pos.x,0,pos.z));
    return it==topOpaqueByColumn_.end() || it->second<pos.y;
}

SmoothLightSample sampleSmoothLighting(const SourceModelQuad& quad,
                                       const BlockInstance& block,
                                       const SmoothLightNeighborhood& neighborhood) {
    SmoothLightSample result;
    const auto normal = faceStep(quad.normal);
    const std::array<int,3> tangentAxes = normal[0] != 0 ? std::array<int,3>{1,2,-1} :
        normal[1] != 0 ? std::array<int,3>{0,2,-1} : std::array<int,3>{0,1,-1};
    const int axisA=tangentAxes[0], axisB=tangentAxes[1];
    const BlockPos outside{block.pos.x+normal[0],block.pos.y+normal[1],block.pos.z+normal[2]};
    const float face = cardinalShade(normal);

    for (std::size_t vertex=0; vertex<4; ++vertex) {
        const auto& p=quad.positions[vertex];
        const int signA = p[axisA] < 0.5f ? -1 : 1;
        const int signB = p[axisB] < 0.5f ? -1 : 1;
        // Sample points lie just outside the face and just inside the corner
        // blocks, matching BlockModelLighter's side/diagonal neighborhood.
        std::array<double,3> local{p[0],p[1],p[2]};
        local[static_cast<std::size_t>(axisA)] = signA < 0 ? 0.98 : 0.02;
        local[static_cast<std::size_t>(axisB)] = signB < 0 ? 0.98 : 0.02;
        local[static_cast<std::size_t>(normal[0]!=0?0:normal[1]!=0?1:2)] = normal[0]+normal[1]+normal[2] < 0 ? 0.98 : 0.02;
        auto blocked=[&](int da,int db) {
            const BlockPos pos{outside.x+(axisA==0?da:axisB==0?db:0),
                               outside.y+(axisA==1?da:axisB==1?db:0),
                               outside.z+(axisA==2?da:axisB==2?db:0)};
            auto q=local;
            q[static_cast<std::size_t>(axisA)] = da==0 ? local[static_cast<std::size_t>(axisA)] : (da<0?0.98:0.02);
            q[static_cast<std::size_t>(axisB)] = db==0 ? local[static_cast<std::size_t>(axisB)] : (db<0?0.98:0.02);
            return neighborhood.occludesLightAt(pos,q[0],q[1],q[2]);
        };
        const bool sideA=blocked(signA,0), sideB=blocked(0,signB);
        const bool corner=(sideA&&sideB) || blocked(signA,signB);
        const int count=static_cast<int>(sideA)+static_cast<int>(sideB)+static_cast<int>(corner);
        // Vanilla smooth AO's four-cell average produces these canonical
        // levels for zero through three corner blockers.
        const float ao=count==0?1.0f:count==1?0.83f:count==2?0.67f:0.55f;

        const auto skyAt=[&](int da,int db) {
            const BlockPos probe{outside.x+(axisA==0?da:axisB==0?db:0),
                                 outside.y+(axisA==1?da:axisB==1?db:0),
                                 outside.z+(axisA==2?da:axisB==2?db:0)};
            return neighborhood.directSkyVisible(probe)?15:0;
        };
        const int sky1=skyAt(signA,0), sky2=skyAt(0,signB), sky3=skyAt(signA,signB);
        const int skyAverage=(sky1+sky2+sky3+skyAt(0,0))/4;
        result.skyLight[vertex]=static_cast<std::uint8_t>(skyAverage);
        result.blockLight[vertex]=0; // Supported material set has no emitting blocks.
        const float skyFactor=0.72f+0.28f*(static_cast<float>(skyAverage)/15.0f);
        result.vertexBrightness[vertex]=std::clamp(face*ao*skyFactor,0.0f,1.0f);
    }
    return result;
}

} // namespace carbon
