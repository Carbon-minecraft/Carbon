#include "carbon/ClientRenderer.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
float minY(const carbon::SourceModelGeometry& geometry) {
    float value=1.0f;
    for (const auto& q:geometry.quads) for (const auto& p:q.positions) value=std::min(value,p[1]);
    return value;
}
float maxY(const carbon::SourceModelGeometry& geometry) {
    float value=0.0f;
    for (const auto& q:geometry.quads) for (const auto& p:q.positions) value=std::max(value,p[1]);
    return value;
}
void checkWinding(const carbon::SourceModelGeometry& geometry) {
    for(const auto& q:geometry.quads) {
        const auto& a=q.positions[0]; const auto& b=q.positions[1]; const auto& c=q.positions[2];
        const std::array<float,3> u{b[0]-a[0],b[1]-a[1],b[2]-a[2]};
        const std::array<float,3> v{c[0]-a[0],c[1]-a[1],c[2]-a[2]};
        const std::array<float,3> cross{u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};
        if(!(cross[0]*q.normal[0]+cross[1]*q.normal[1]+cross[2]*q.normal[2]>0.0f)) {
            std::cerr<<"bad winding: "<<cross[0]<<","<<cross[1]<<","<<cross[2]<<" n="<<q.normal[0]<<","<<q.normal[1]<<","<<q.normal[2]<<'\n';
            assert(false);
        }
    }
}
}

int main(int argc,char** argv) {
    assert(argc==2);
    const std::filesystem::path root=argv[1];
    const auto stone=carbon::loadSourceBlockGeometry(root,"minecraft:stone");
    assert(stone && stone.quads.size()==6);
    for (const auto& face:stone.quads) {
        assert(face.texture=="minecraft:block/stone");
        for (const auto& point:face.positions)
            for (float coordinate:point) assert(coordinate>=0.0f && coordinate<=1.0f);
    }

    const auto bottom=carbon::loadSourceBlockGeometry(root,"minecraft:stone_slab","type=bottom");
    const auto top=carbon::loadSourceBlockGeometry(root,"minecraft:stone_slab","type=top");
    const auto doubled=carbon::loadSourceBlockGeometry(root,"minecraft:stone_slab","type=double");
    assert(bottom && bottom.quads.size()==6 && minY(bottom)==0.0f && maxY(bottom)==0.5f);
    assert(top && top.quads.size()==6 && minY(top)==0.5f && maxY(top)==1.0f);
    assert(doubled && doubled.quads.size()==6 && minY(doubled)==0.0f && maxY(doubled)==1.0f);
    assert(bottom.quads.front().texture=="minecraft:block/stone");
    // Source slab elements declare their exposed side UVs as a half-height
    // rectangle, and that crop is preserved by the geometry compiler.
    bool sawHalfHeightUv=false;
    for (const auto& face:bottom.quads) if (face.cullFace && *face.cullFace=="north") {
        const auto [lo,hi]=std::minmax_element(face.uvs.begin(),face.uvs.end(),
            [](const auto& a,const auto& b){return a[1]<b[1];});
        sawHalfHeightUv=((*hi)[1]-(*lo)[1])==8.0f;
    }
    assert(sawHalfHeightUv);

    const auto grass=carbon::loadSourceBlockGeometry(root,"minecraft:grass_block","snowy=false");
    assert(grass && grass.quads.size()>6);
    assert(std::any_of(grass.quads.begin(),grass.quads.end(),[](const auto& face) {
        return face.texture=="minecraft:block/grass_block_side_overlay" && face.tintIndex>=0;
    }));

    // The supplied fence source blockstate selects an unconditional post and
    // adds one arm model for each active horizontal connection.
    const auto fenceNone=carbon::loadSourceBlockGeometry(root,"minecraft:oak_fence","north=false,east=false,south=false,west=false");
    const auto fenceNorth=carbon::loadSourceBlockGeometry(root,"minecraft:oak_fence","north=true,east=false,south=false,west=false");
    const auto fenceCross=carbon::loadSourceBlockGeometry(root,"minecraft:oak_fence","north=true,east=true,south=true,west=true");
    assert(fenceNone && fenceNorth && fenceCross);
    assert(fenceNorth.quads.size()>fenceNone.quads.size());
    assert(fenceCross.quads.size()>fenceNorth.quads.size());
    for(const auto& q:fenceCross.quads) assert(q.texture=="minecraft:block/oak_planks");
    checkWinding(fenceCross);

    // Native fixtures exercise nested AND/OR, false predicates, and several
    // simultaneously active multipart parts using source-style conditions.
    const auto multipartFixture=std::filesystem::temp_directory_path()/"carbon-multipart-fixture";
    std::filesystem::remove_all(multipartFixture);
    std::filesystem::create_directories(multipartFixture/"assets/minecraft/models/block");
    std::filesystem::create_directories(multipartFixture/"assets/minecraft/blockstates");
    { std::ofstream model(multipartFixture/"assets/minecraft/models/block/a.json"); model << R"({"textures":{"all":"minecraft:block/stone"},"elements":[{"from":[0,0,0],"to":[8,8,8],"faces":{"up":{"texture":"#all"}}}]})"; }
    { std::ofstream model(multipartFixture/"assets/minecraft/models/block/b.json"); model << R"({"textures":{"all":"minecraft:block/stone"},"elements":[{"from":[8,8,8],"to":[16,16,16],"faces":{"up":{"texture":"#all"}}}]})"; }
    { std::ofstream state(multipartFixture/"assets/minecraft/blockstates/multipart_fixture.json"); state << R"({"multipart":[{"apply":{"model":"minecraft:block/a"}},{"when":{"north":"true","OR":[{"east":"true"},{"AND":[{"south":"true"},{"west":"true"}]}]},"apply":{"model":"minecraft:block/b"}},{"when":{"north":"false"},"apply":{"model":"minecraft:block/a"}}]})"; }
    const auto multipartTrue=carbon::loadSourceBlockGeometry(multipartFixture,"minecraft:multipart_fixture","north=true,east=true,south=false,west=false");
    const auto multipartFalse=carbon::loadSourceBlockGeometry(multipartFixture,"minecraft:multipart_fixture","north=false,east=false,south=false,west=false");
    const auto multipartNested=carbon::loadSourceBlockGeometry(multipartFixture,"minecraft:multipart_fixture","north=true,east=false,south=true,west=true");
    assert(multipartTrue && multipartFalse && multipartNested);
    assert(multipartTrue.quads.size()==2 && multipartNested.quads.size()==2 && multipartFalse.quads.size()==2);
    assert(multipartTrue.quads[0].positions!=multipartTrue.quads[1].positions);
    assert(multipartFalse.quads[0].positions==multipartFalse.quads[1].positions);
    std::filesystem::remove_all(multipartFixture);

    // Source element transforms include both a 22.5 degree tilt and a 45
    // degree rescaled rotation around the declared origin.
    const auto elementFixture=std::filesystem::temp_directory_path()/"carbon-element-rotation-fixture";
    std::filesystem::remove_all(elementFixture);
    std::filesystem::create_directories(elementFixture/"assets/minecraft/models/block");
    { std::ofstream model(elementFixture/"assets/minecraft/models/block/tilt.json"); model << R"({"textures":{"all":"minecraft:block/stone"},"elements":[{"from":[4,4,4],"to":[12,12,12],"rotation":{"origin":[8,8,8],"axis":"x","angle":22.5},"faces":{"up":{"texture":"#all","cullface":"up"}}},{"from":[4,4,4],"to":[12,12,12],"rotation":{"origin":[8,8,8],"axis":"z","angle":45,"rescale":true},"faces":{"north":{"texture":"#all"}}}]})"; }
    const auto elementGeometry=carbon::loadSourceModelGeometry(elementFixture,"minecraft:block/tilt");
    assert(elementGeometry && elementGeometry.quads.size()==2);
    checkWinding(elementGeometry);
    assert(elementGeometry.quads[0].normal[1]<1.0f && elementGeometry.quads[0].normal[2]>0.0f);
    float minX=1.0f,maxX=0.0f;
    for(const auto& p:elementGeometry.quads[1].positions) { minX=std::min(minX,p[0]); maxX=std::max(maxX,p[0]); }
    assert(minX<0.25f && maxX>0.75f);
    std::filesystem::remove_all(elementFixture);

    const auto stairsStraight=carbon::loadSourceBlockGeometry(root,"minecraft:oak_stairs","facing=east,half=bottom,shape=straight");
    const auto stairsTurned=carbon::loadSourceBlockGeometry(root,"minecraft:oak_stairs","facing=north,half=bottom,shape=straight");
    const auto stairsUvLocked=carbon::loadSourceBlockGeometry(root,"minecraft:oak_stairs","facing=east,half=top,shape=straight");
    if(!stairsStraight) std::cerr<<"stairs east: "<<stairsStraight.error<<'\n';
    if(!stairsTurned) std::cerr<<"stairs north: "<<stairsTurned.error<<'\n';
    if(!stairsUvLocked) std::cerr<<"stairs uv: "<<stairsUvLocked.error<<'\n';
    assert(stairsStraight && stairsTurned && stairsUvLocked);
    assert(stairsStraight.quads.size()>6 && stairsTurned.quads.size()==stairsStraight.quads.size());
    assert(stairsUvLocked.quads.size()==stairsStraight.quads.size());
    assert(minY(stairsStraight)==0.0f && maxY(stairsStraight)==1.0f);
    assert(minY(stairsUvLocked)==0.0f && maxY(stairsUvLocked)==1.0f);
    checkWinding(stairsStraight); checkWinding(stairsTurned); checkWinding(stairsUvLocked);
    // Exercise the transform compiler against both uvlock modes on a fixture
    // with identical geometry and a 90 degree source blockstate rotation.
    const auto fixture=std::filesystem::temp_directory_path()/"carbon-transform-fixture";
    std::filesystem::remove_all(fixture);
    std::filesystem::create_directories(fixture/"assets/minecraft/models/block");
    std::filesystem::create_directories(fixture/"assets/minecraft/blockstates");
    { std::ofstream model(fixture/"assets/minecraft/models/block/quad.json"); model << R"({"textures":{"all":"minecraft:block/stone"},"elements":[{"from":[2,3,4],"to":[14,13,12],"faces":{"north":{"texture":"#all","cullface":"north"}}}]})"; }
    { std::ofstream state(fixture/"assets/minecraft/blockstates/uv.json"); state << R"({"variants":{"mode=lock":{"model":"minecraft:block/quad","x":90,"uvlock":true},"mode=free":{"model":"minecraft:block/quad","x":90}}})"; }
    const auto locked=carbon::loadSourceBlockGeometry(fixture,"minecraft:uv","mode=lock");
    const auto free=carbon::loadSourceBlockGeometry(fixture,"minecraft:uv","mode=free");
    if(!locked) std::cerr<<"locked fixture: "<<locked.error<<'\n';
    if(!free) std::cerr<<"free fixture: "<<free.error<<'\n';
    assert(locked && free && locked.quads.size()==1 && free.quads.size()==1);
    assert(locked.quads[0].positions==free.quads[0].positions);
    assert(locked.quads[0].uvs!=free.quads[0].uvs);
    assert(locked.quads[0].cullFace==std::optional<std::string>("up"));
    checkWinding(locked); checkWinding(free);
    std::filesystem::remove_all(fixture);
    // Position-seeded weighted alternatives match on repeat and vary across a
    // sweep; stone's source blockstate has four equally weighted source models.
    const auto a=carbon::loadSourceBlockGeometry(root,"minecraft:stone",{},2,4,7);
    const auto again=carbon::loadSourceBlockGeometry(root,"minecraft:stone",{},2,4,7);
    assert(a && again && a.quads.size()==again.quads.size());
    assert(a.quads.front().positions==again.quads.front().positions);
    bool foundDifferent=false;
    for (int x=-64;x<64;++x) {
        const auto candidate=carbon::loadSourceBlockGeometry(root,"minecraft:stone",{},x,4,7);
        assert(candidate);
        if (candidate.quads.front().positions!=a.quads.front().positions) foundDifferent=true;
    }
    assert(foundDifferent);
    std::cout << "PASS: source block faces, slab geometry, stairs rotations/uvlock, and seeded weighted variants\n";
}
