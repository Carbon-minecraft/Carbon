#include "carbon/SmoothLighting.hpp"

#include <cassert>
#include <cmath>

using namespace carbon;

static void assertNear(float actual, float expected) {
    assert(std::abs(actual - expected) < 1.0e-5f);
}

int main() {
    const BlockInstance center{{0,0,0},BlockId::Stone};
    SourceModelQuad top;
    top.normal={0,1,0};
    top.positions={{{0,1,1},{1,1,1},{1,1,0},{0,1,0}}};

    const BlockInstance open[] = {center};
    const SmoothLightNeighborhood openNeighborhood(open);
    auto lit=sampleSmoothLighting(top,center,openNeighborhood);
    for(float brightness:lit.vertexBrightness) assert(brightness==1.0f);
    for(auto sky:lit.skyLight) assert(sky==15);

    const BlockInstance corner[] = {center,{{1,1,0},BlockId::Stone}};
    const SmoothLightNeighborhood cornerNeighborhood(corner);
    auto shaded=sampleSmoothLighting(top,center,cornerNeighborhood);
    assert(shaded.vertexBrightness[1] < lit.vertexBrightness[1]);
    assert(shaded.vertexBrightness[1] < shaded.vertexBrightness[0]);
    // Indexed fixture from the pre-cache path: one AO side blocker, while
    // direct sky sees 3 of 4 probe columns (11/15).
    assertNear(shaded.vertexBrightness[2], 0.83f * (0.72f + 0.28f * (11.0f / 15.0f)));
    assert(shaded.skyLight[2] == 11);

    const BlockInstance covered[] = {center,{{0,1,0},BlockId::Stone}};
    const SmoothLightNeighborhood coveredNeighborhood(covered);
    auto roofed=sampleSmoothLighting(top,center,coveredNeighborhood);
    for(auto sky:roofed.skyLight) assert(sky<15);
    for(float brightness:roofed.vertexBrightness) assert(brightness<lit.vertexBrightness[0]);

    const BlockInstance partial[] = {center,{{1,1,0},BlockId::StoneSlab,SlabType::Bottom}};
    const SmoothLightNeighborhood partialNeighborhood(partial);
    auto partialLight=sampleSmoothLighting(top,center,partialNeighborhood);
    assert(partialLight.vertexBrightness[1] < lit.vertexBrightness[1]);
    assertNear(partialLight.vertexBrightness[2], shaded.vertexBrightness[2]);

    const BlockInstance fence[] = {center,{{1,1,0},BlockId::OakFence}};
    const SmoothLightNeighborhood fenceNeighborhood(fence);
    const auto fenceLight=sampleSmoothLighting(top,center,fenceNeighborhood);
    // Oak fence collision/outline posts are not light occluders in this slice.
    assertNear(fenceLight.vertexBrightness[2], lit.vertexBrightness[2]);
    assert(fenceLight.skyLight[2] == 15);

    const BlockInstance upperStair[] = {center,{{1,1,0},BlockId::StoneStairs,
        SlabType::Bottom,Direction::North,StairHalf::Top}};
    const SmoothLightNeighborhood stairNeighborhood(upperStair);
    const auto stairLight=sampleSmoothLighting(top,center,stairNeighborhood);
    // Upper stair geometry leaves the low corner probe open, but the block
    // still participates in the source column-occlusion sky query.
    assertNear(stairLight.vertexBrightness[2], 0.72f + 0.28f * (11.0f / 15.0f));
    assert(stairLight.skyLight[2] == 11);
}
