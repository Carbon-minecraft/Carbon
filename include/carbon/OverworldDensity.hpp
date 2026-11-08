#pragma once

// Data-backed scalar evaluator for Java Edition density-function JSON assets.
// Unsupported/contextual operators fail explicitly; callers must supply the
// original seeded noise and structure/blending context.
#include <filesystem>
#include <functional>
#include <memory>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "nlohmann/json.hpp"
#include "carbon/OverworldNoise.hpp"
#include "carbon/BlendedTerrainNoise.hpp"

namespace carbon {

struct DensityPos { int x{}, y{}, z{}; };
struct DensityContext {
    std::function<float(const std::string&, double, double, double)> noise;
    std::function<float(const nlohmann::json&, DensityPos)> external;
    struct MemoKey { const void* node{}; int x{},y{},z{}; bool operator==(const MemoKey&) const = default; };
    struct MemoHash { std::size_t operator()(const MemoKey& k) const noexcept; };
    std::shared_ptr<std::unordered_map<MemoKey,float,MemoHash>> memo =
        std::make_shared<std::unordered_map<MemoKey,float,MemoHash>>();
};

struct RawDensityChunk {
    static constexpr int MIN_Y=-64;
    static constexpr int HEIGHT=384;
    int chunkX{},chunkZ{};
    std::vector<float> density; // pre-aquifer, pre-material source density
    float at(int localX,int worldY,int localZ) const;
    int surfaceY(int localX,int localZ) const;
};

class OverworldDensityGraph {
public:
    explicit OverworldDensityGraph(std::filesystem::path dataRoot);
    float sample(const std::string& key, DensityPos pos, const DensityContext& context) const;
    float sampleExpression(const nlohmann::json& expression, DensityPos pos,
                           const DensityContext& context) const;
    static std::string firstUnsupported(const std::filesystem::path& dataRoot,
                                        const std::string& key);
private:
    std::filesystem::path root_;
    mutable std::unordered_map<std::string, nlohmann::json> files_;
    const nlohmann::json& load(const std::string& key) const;
    float eval(const nlohmann::json& node, DensityPos pos, const DensityContext& context,
               int recursion) const;
    float evalImpl(const nlohmann::json& node, DensityPos pos, const DensityContext& context,
                   int recursion) const;
    float evalSpline(const nlohmann::json& spline, DensityPos pos,
                     const DensityContext& context, int recursion) const;
};

// Complete default-world density router for new worlds with an empty Blender
// and structures disabled. Aquifer and material decisions are separate stages.
class SeededOverworldDensity {
public:
    class Session {
    public:
        float function(const std::string& identifier,int blockX,int blockY,int blockZ) const;
        float finalDensity(int blockX,int blockY,int blockZ) const;
        float noise(const std::string& identifier,double x,double y,double z) const;
    private:
        friend class SeededOverworldDensity;
        Session(const SeededOverworldDensity* owner,DensityContext context):owner_(owner),context_(std::move(context)){}
        const SeededOverworldDensity* owner_;
        DensityContext context_;
    };
    SeededOverworldDensity(std::int64_t seed, std::filesystem::path decompiledRoot);
    Session beginSession() const;
    float finalDensity(int blockX,int blockY,int blockZ) const;
    float function(const std::string& identifier,int blockX,int blockY,int blockZ) const;
    float noise(const std::string& identifier,double x,double y,double z) const;
    XoroshiroPositionalRandomFactory positionalFactory(const std::string& name) const;
    RawDensityChunk densityChunk(int chunkX,int chunkZ) const;
private:
    std::int64_t seed_;
    OverworldDensityGraph graph_;
    mutable OverworldNoise noises_;
    XoroshiroPositionalRandomFactory positional_;
    mutable std::unique_ptr<BlendedTerrainNoise> blended_;
    DensityContext makeContext() const;
};

}
