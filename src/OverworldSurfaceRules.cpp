#include "carbon/OverworldSurfaceRules.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace carbon {
namespace {
std::string stripNamespace(std::string_view id) {
    constexpr std::string_view prefix="minecraft:";
    if(id.starts_with(prefix)) return std::string(id.substr(prefix.size()));
    return std::string(id);
}
std::string typeOf(const nlohmann::json& node) {
    if(!node.is_object() || !node.contains("type") || !node["type"].is_string()) return {};
    return node["type"].get<std::string>();
}
}

PlainsSurfaceRules::PlainsSurfaceRules(std::filesystem::path root)
    : dataRoot_(std::move(root)/"data"/"minecraft"/"worldgen") {
    try {
        std::ifstream in(dataRoot_/"world_preset"/"single_biome_surface.json",std::ios::binary);
        if(!in) throw std::runtime_error("missing source world_preset/single_biome_surface.json");
        const auto preset=nlohmann::json::parse(in);
        const auto& generator=preset.at("dimensions").at("minecraft:overworld").at("generator");
        presetMatchesSource_=generator.at("type")=="minecraft:noise" &&
            generator.at("settings")=="minecraft:overworld" &&
            generator.at("biome_source").at("type")=="minecraft:fixed" &&
            generator.at("biome_source").at("biome")=="minecraft:plains";
        if(!presetMatchesSource_) error_="Source single_biome_surface preset is not fixed minecraft:plains with minecraft:overworld settings";
    } catch(const std::exception& e) { error_=e.what(); }
}

const nlohmann::json* PlainsSurfaceRules::loadJson(std::string_view root,std::string_view id) {
    const std::string key=std::string(root)+"/"+stripNamespace(id);
    if(auto it=jsonCache_.find(key);it!=jsonCache_.end()) return &it->second;
    const auto path=dataRoot_/std::filesystem::path(key+".json");
    try {
        std::ifstream input(path,std::ios::binary);
        if(!input) throw std::runtime_error("missing source material data: "+path.string());
        auto [it,inserted]=jsonCache_.emplace(key,nlohmann::json::parse(input));
        (void)inserted; return &it->second;
    } catch(const std::exception& e) { error_=e.what(); return nullptr; }
}

bool PlainsSurfaceRules::conditionReference(std::string_view id,const MaterialRuleSample& s,int depth) {
    if(depth>64) { error_="source material condition reference recursion limit"; return false; }
    const auto* node=loadJson("material_condition",id);
    return node && condition(*node,s,depth+1);
}

bool PlainsSurfaceRules::condition(const nlohmann::json& n,const MaterialRuleSample& s,int depth) {
    if(depth>64) { error_="source material condition recursion limit"; return false; }
    if(n.is_string()) return conditionReference(n.get<std::string>(),s,depth+1);
    if(!n.is_object()) return false;
    const auto type=typeOf(n);
    if(type=="minecraft:biome") {
        const auto& values=n.at("biome_is");
        if(values.is_string()) return values.get<std::string>()==s.biome;
        if(values.is_array()) return std::find(values.begin(),values.end(),s.biome)!=values.end();
        return false;
    }
    if(type=="minecraft:not") return !condition(n.at("invert"),s,depth+1);
    if(type=="minecraft:water") {
        const bool addStone=n.value("add_stone_depth",false);
        const int offset=n.value("offset",0), multiplier=n.value("surface_depth_multiplier",0);
        return s.waterHeight==std::numeric_limits<int>::min() ||
            s.blockY+(addStone?s.stoneDepthAbove:0)>=s.waterHeight+offset+s.surfaceDepth*multiplier;
    }
    if(type=="minecraft:stone_depth") {
        const int offset=n.value("offset",0), secondaryRange=n.value("secondary_depth_range",0);
        const bool addSurface=n.value("add_surface_depth",false);
        const bool ceiling=n.value("surface_type",std::string("floor"))=="ceiling";
        const int stoneDepth=ceiling?s.stoneDepthBelow:s.stoneDepthAbove;
        const int secondary=secondaryRange==0?0:static_cast<int>((s.surfaceSecondary+1.0)*0.5*secondaryRange);
        return stoneDepth<=1+offset+(addSurface?s.surfaceDepth:0)+secondary;
    }
    if(type=="minecraft:y_above") {
        const auto& anchor=n.at("anchor");
        int y=anchor.value("absolute",0);
        const int surfaceDepthMultiplier=n.value("surface_depth_multiplier",0);
        return s.blockY+(n.value("add_stone_depth",false)?s.stoneDepthAbove:0)>=y+s.surfaceDepth*surfaceDepthMultiplier;
    }
    if(type=="minecraft:noise_threshold") {
        double value=s.surfaceNoise;
        const auto noise=n.value("noise",std::string("minecraft:surface"));
        if(s.noise3d) value=s.noise3d(noise,s.blockY,s.blockY,s.blockY);
        return value>=n.value("min_threshold",-std::numeric_limits<double>::max()) && value<=n.value("max_threshold",std::numeric_limits<double>::max());
    }
    if(type=="minecraft:steep") return s.steep || s.gradientX<=-4 || s.gradientZ>=4;
    if(type=="minecraft:hole") return s.hole;
    if(type=="minecraft:above_preliminary_surface") return s.abovePreliminarySurface;
    if(type=="minecraft:temperature") return s.notUnderwater;
    return false;
}

std::optional<std::string> PlainsSurfaceRules::ruleReference(std::string_view id,const MaterialRuleSample& sample,int depth) {
    if(depth>64) { error_="source material rule reference recursion limit"; return {}; }
    const auto* node=loadJson("material_rule",id);
    return node?rule(*node,sample,depth+1):std::optional<std::string>{};
}

std::optional<std::string> PlainsSurfaceRules::rule(const nlohmann::json& n,const MaterialRuleSample& s,int depth) {
    if(depth>64) { error_="source material rule recursion limit"; return {}; }
    if(n.is_string()) return ruleReference(n.get<std::string>(),s,depth+1);
    if(!n.is_object()) return {};
    const auto type=typeOf(n);
    if(type=="minecraft:block") {
        if(!n.contains("result_state")||!n["result_state"].is_string()) return {};
        return n["result_state"].get<std::string>();
    }
    if(type=="minecraft:sequence") {
        const auto& items=n.at("sequence");
        for(const auto& item:items) if(auto found=rule(item,s,depth+1)) return found;
        return {};
    }
    if(type=="minecraft:condition") {
        if(condition(n.at("if_true"),s,depth+1)) return rule(n.at("then_run"),s,depth+1);
        return {};
    }
    return {};
}

std::optional<std::string> PlainsSurfaceRules::surfaceBlockAt(const MaterialRuleSample& sample) {
    if(!presetMatchesSource_ || sample.biome!="minecraft:plains") return {};
    // These are the exact roots selected by the 26.3 overworld surface rule:
    // `on_floor -> minecraft:overworld/biome_surface` and
    // `under_floor -> minecraft:overworld/under_biome_surface`.
    if(sample.onFloor) return ruleReference("minecraft:overworld/biome_surface",sample,0);
    if(sample.underFloor) return ruleReference("minecraft:overworld/under_biome_surface",sample,0);
    return {};
}

} // namespace carbon
