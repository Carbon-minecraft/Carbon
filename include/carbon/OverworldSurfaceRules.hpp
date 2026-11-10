#pragma once

#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace carbon {

// Inputs correspond to the source MaterialRuleContext at one solid block.
// The chunk filler owns heightmap/depth/water detection and passes those
// results through; this resolver interprets the supplied 26.3 rule JSON.
struct MaterialRuleSample {
    std::string biome{"minecraft:plains"};
    int blockY{};
    int seaLevel{63};
    int stoneDepthAbove{};
    int stoneDepthBelow{};
    int surfaceDepth{};
    int waterHeight{std::numeric_limits<int>::min()};
    int gradientX{}, gradientZ{};
    double surfaceSecondary{}, surfaceNoise{};
    bool onFloor{};
    bool underFloor{};
    bool deepUnderFloor{};
    bool veryDeepUnderFloor{};
    bool notUnderwater{true};
    bool notUnderDeepWater{true};
    bool onCeiling{};
    bool hole{};
    bool steep{};
    bool abovePreliminarySurface{};
    std::function<double(std::string_view, int, int, int)> noise3d;
};

// Source-data-backed interpreter for the biome surface rules selected by the
// supplied vanilla Overworld presets. It reads original material-rule JSON.
class PlainsSurfaceRules {
public:
    explicit PlainsSurfaceRules(std::filesystem::path decompiledRoot);
    bool presetMatchesSource() const noexcept { return presetMatchesSource_; }
    bool normalPresetMatchesSource() const noexcept { return normalPresetMatchesSource_; }
    const std::string& error() const noexcept { return error_; }
    std::optional<std::string> surfaceBlockAt(const MaterialRuleSample& sample);

private:
    std::filesystem::path dataRoot_;
    bool presetMatchesSource_{};
    bool normalPresetMatchesSource_{};
    std::string error_;
    std::map<std::string, nlohmann::json, std::less<>> jsonCache_;
    const nlohmann::json* loadJson(std::string_view root, std::string_view id);
    bool condition(const nlohmann::json& node, const MaterialRuleSample& sample, int depth);
    std::optional<std::string> rule(const nlohmann::json& node, const MaterialRuleSample& sample, int depth);
    std::optional<std::string> ruleReference(std::string_view id, const MaterialRuleSample& sample, int depth);
    bool conditionReference(std::string_view id, const MaterialRuleSample& sample, int depth);
};

} // namespace carbon
