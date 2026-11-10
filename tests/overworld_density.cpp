#include "carbon/OverworldDensity.hpp"
#include <cassert>
#include <bit>
#include <cmath>
#include <iostream>
#include <iomanip>

int main(int argc,char** argv) {
    assert(argc==2);
    carbon::OverworldDensityGraph graph(argv[1]);
    carbon::DensityContext context;
    context.noise=[](const std::string&,double,double,double){return 0.0F;};
    context.external=[](const nlohmann::json& node,carbon::DensityPos){
        const auto type=node.value("type","");
        return type=="minecraft:blend_alpha"?1.0F:0.0F;
    };
    // The graph is loaded from the unmodified 26.3 data tree. With the test
    // provider fixed to zero, the continentalness noise node resolves to zero.
    assert(graph.sample("overworld/continents",{0,0,0},context)==0.0F);
    assert(graph.sample("overworld/continents",{-97,0,161},context)==0.0F);
    // This exercises the complete reachable default final-density JSON graph,
    // including nested data splines and 4x8 interpolation. Contextual noise,
    // blender and structure inputs here are deliberately fixed test providers;
    // it does not claim a seeded terrain oracle comparison.
    const float a=graph.sample("overworld/final_density",{-17,-20,31},context);
    const float b=graph.sample("overworld/final_density",{-17,-20,31},context);
    assert(std::isfinite(a)&&a==b);
    const nlohmann::json interpolated={
        {"type","minecraft:interpolated"},{"cell_size_xz",4},{"cell_size_y",8},
        {"input",{{"type","minecraft:gradient"},{"axis","y"},{"from_coordinate",0},
                  {"from_value",0.0},{"to_coordinate",8},{"to_value",8.0}}}
    };
    assert(std::abs(graph.sampleExpression(interpolated,{2,4,2},context)-4.0F)<1e-6F);
    nlohmann::json topSurface={
        {"type","minecraft:find_top_surface"},{"cell_height",8},{"lower_bound",-8},
        {"upper_bound",{{"type","minecraft:constant"},{"argument",16.0}}},
        {"density",{{"type","minecraft:gradient"},{"axis","y"},
                     {"from_coordinate",0},{"from_value",-1.0},
                     {"to_coordinate",16},{"to_value",1.0}}}
    };
    assert(graph.sampleExpression(topSurface,{3,0,-2},context)==16.0F);
    context.memo=std::make_shared<std::unordered_map<carbon::DensityContext::MemoKey,
                                                    float,carbon::DensityContext::MemoHash>>();
    topSurface["upper_bound"]["argument"]=15.9;
    assert(graph.sampleExpression(topSurface,{3,0,-2},context)==-8.0F);
    context.memo=std::make_shared<std::unordered_map<carbon::DensityContext::MemoKey,
                                                    float,carbon::DensityContext::MemoHash>>();
    topSurface["lower_bound"]=-16;
    topSurface["upper_bound"]["argument"]=-0.1;
    topSurface["density"]["type"]="minecraft:constant";
    topSurface["density"]["argument"]=1.0;
    assert(graph.sampleExpression(topSurface,{3,0,-2},context)==-8.0F);
    carbon::SeededOverworldDensity seeded(987654321,argv[1]);
    carbon::SeededOverworldDensity repeat(987654321,argv[1]);
    const auto density=seeded.finalDensity(-17,-20,31);
    assert(std::isfinite(density));
    assert(std::bit_cast<std::uint32_t>(density)==
           std::bit_cast<std::uint32_t>(repeat.finalDensity(-17,-20,31)));
    const float acrossChunk=seeded.finalDensity(-16,-20,31);
    assert(std::isfinite(acrossChunk));
    auto raw=seeded.densityChunk(-2,3);
    assert(raw.at(0,-20,0)==seeded.finalDensity(-32,-20,48));
    assert(raw.surfaceY(0,0)>=carbon::RawDensityChunk::MIN_Y);
    // The generated-chunk sampler precomputes source interpolated density
    // volumes from 4x8 cell corners. Verify the optimized session preserves
    // the unprepared graph's exact float bits at negative-coordinate edges,
    // negative-Y cell seams, and the top of the native world volume.
    auto direct=seeded.beginSession();
    auto prepared=seeded.beginSession();
    prepared.prepareFinalDensityChunk(-2,3);
    for (const int x : {-32,-31,-29,-28,-27,-17,-16})
        for (const int z : {48,49,51,52,53,62,63})
            for (const int y : {-64,-63,-57,-56,-1,0,7,8,319})
                assert(std::bit_cast<std::uint32_t>(direct.finalDensity(x,y,z)) ==
                       std::bit_cast<std::uint32_t>(prepared.finalDensity(x,y,z)));
    carbon::SeededOverworldDensity probes(42,argv[1]);
    std::cout<<std::setprecision(9);
    for(const auto p : {carbon::DensityPos{0,64,0},carbon::DensityPos{0,128,0},
                        carbon::DensityPos{-1,-64,0},carbon::DensityPos{-16,-64,0},
                        carbon::DensityPos{-15,-64,0},carbon::DensityPos{15,319,0}}) {
        const float preliminary=probes.function("overworld/preliminary_surface_level",p.x,0,p.z);
        assert(preliminary>=-64.0F&&preliminary<=320.0F);
        assert(static_cast<int>(preliminary)%8==0);
        std::cout<<"seed42 final_density "<<p.x<<","<<p.y<<","<<p.z<<"="
                 <<probes.finalDensity(p.x,p.y,p.z)<<" preliminary_surface="
                 <<preliminary<<"\n";
    }
    std::cout<<"seed42 chunk_surface_level 0,0="<<probes.function("overworld/chunk_surface_level",0,0,0)<<"\n";
    std::cout<<"26.3 JSON density graph subset ok\n";
}
