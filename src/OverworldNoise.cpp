#include "carbon/OverworldNoise.hpp"
#include <algorithm>
#include <stdexcept>

namespace carbon {
namespace {
struct Spec { int first; std::vector<double> amplitudes; };
const Spec& spec(const std::string& id) {
    // Values are transcribed from the supplied 26.3 NoiseData.bootstrap.
    static const std::unordered_map<std::string,Spec> specs{
        {"temperature",{-10,{1.5,0,1,0,0,0}}}, {"vegetation",{-8,{1,1,0,0,0,0}}},
        {"continentalness",{-9,{1,1,2,2,2,1,1,1,1}}}, {"erosion",{-9,{1,1,0,1,1}}},
        {"ridge",{-7,{1,2,1,0,0,0}}}, {"offset",{-3,{1}}},
        {"aquifer_barrier",{-3,{1}}}, {"aquifer_fluid_level_floodedness",{-7,{1}}},
        {"aquifer_lava",{-1,{1}}}, {"aquifer_fluid_level_spread",{-5,{1}}},
        {"cave_entrance",{-7,{0.4,0.5,1}}}, {"cave_layer",{-8,{1}}},
        {"cave_cheese",{-8,{0.5,1,2,1,2,1,0,2,0}}},
        {"pillar",{-7,{1,1}}}, {"pillar_rareness",{-8,{1}}}, {"pillar_thickness",{-8,{1}}},
        {"spaghetti_2d",{-7,{1}}}, {"spaghetti_2d_elevation",{-8,{1}}},
        {"spaghetti_2d_modulator",{-11,{1}}}, {"spaghetti_2d_thickness",{-11,{1}}},
        {"spaghetti_3d_1",{-7,{1}}}, {"spaghetti_3d_2",{-7,{1}}},
        {"spaghetti_3d_rarity",{-11,{1}}}, {"spaghetti_3d_thickness",{-8,{1}}},
        {"spaghetti_roughness",{-5,{1}}}, {"spaghetti_roughness_modulator",{-8,{1}}},
        {"noodle",{-8,{1}}}, {"noodle_thickness",{-8,{1}}},
        {"noodle_ridge_a",{-7,{1}}}, {"noodle_ridge_b",{-7,{1}}},
        {"jagged",{-16,{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}}},
        {"temperature_large",{-12,{1.5,0,1,0,0,0}}}, {"vegetation_large",{-10,{1,1,0,0,0,0}}},
        {"continentalness_large",{-11,{1,1,2,2,2,1,1,1,1}}}, {"erosion_large",{-11,{1,1,0,1,1}}}
        ,{"surface",{-6,{1,1,1}}}, {"surface_secondary",{-6,{1,1,0,1}}},
        {"surface_swamp",{-2,{1}}}, {"calcite",{-9,{1,1,1}}}, {"gravel",{-8,{1,1,1,1}}},
        {"powder_snow",{-6,{1,1,1}}}, {"packed_ice",{-7,{1,1,1}}}, {"ice",{-4,{1,1,1}}},
        {"clay_bands_offset",{-8,{1}}}, {"badlands_pillar",{-2,{1,1,1,1}}},
        {"badlands_pillar_roof",{-8,{1}}}, {"badlands_surface",{-6,{1,1,1}}},
        {"iceberg_pillar",{-6,{1,1,1}}}, {"iceberg_pillar_roof",{-3,{1}}},
        {"iceberg_surface",{-6,{1,1,1}}}, {"sulfur_cave_gradient",{-5,{1,0,1}}}
    };
    const auto it=specs.find(id);
    if(it==specs.end()) throw std::runtime_error("unsupported 26.3 noise registry key: "+id);
    return it->second;
}
std::string unnamespace(std::string id) {
    if(id.rfind("minecraft:",0)==0) id.erase(0,10);
    return id;
}
}

OverworldNoise::OverworldNoise(std::int64_t seed)
    : random_([&](){XoroshiroRandomSource source(seed);return source.forkPositionalSeed();}()) {}

XoroshiroNormalNoise& OverworldNoise::get(const std::string& raw) {
    const auto id=unnamespace(raw);
    auto it=instances_.find(id);
    if(it!=instances_.end()) return *it->second;
    const auto& s=spec(id);
    NormalNoise params=NormalNoise::createParity(s.first,s.amplitudes);
    auto source=random_.fromHashOf("minecraft:"+id);
    auto noise=std::make_unique<XoroshiroNormalNoise>(source,params);
    return *instances_.emplace(id,std::move(noise)).first->second;
}

float OverworldNoise::sample(const std::string& identifier,double x,double y,double z) {
    return get(identifier).get(x,y,z);
}
}
