#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace carbon {

struct ModelFace {
    std::string texture; // Resolved resource identifier, e.g. minecraft:block/stone
    std::optional<std::string> cullFace;
    int tintIndex = -1;
    std::optional<std::array<float, 4>> uv;
    int rotation = 0;
};

struct ModelElement {
    std::array<float, 3> from{};
    std::array<float, 3> to{};
    struct Rotation {
        std::array<float, 3> origin{8.0f, 8.0f, 8.0f};
        std::string axis;
        float angle{};
        bool rescale{};
    };
    std::optional<Rotation> rotation;
    std::map<std::string, ModelFace, std::less<>> faces;
};

struct BlockModel {
    std::string id;
    std::optional<std::string> parent;
    std::map<std::string, std::string, std::less<>> textures;
    std::vector<ModelElement> elements;
    std::optional<std::string> particle;
    bool ambientOcclusion = true;
};

struct ModelLoadResult {
    std::optional<BlockModel> model;
    std::string error;
    explicit operator bool() const noexcept { return model.has_value(); }
};

struct BlockModelVariant {
    std::string model;
    int xRotation = 0;
    int yRotation = 0;
    int zRotation = 0;
    bool uvLock = false;
    int weight = 1;
};

struct BlockStateModels {
    std::map<std::string, std::vector<BlockModelVariant>, std::less<>> variants;
    struct Condition {
        enum class Kind { Property, All, Any } kind{Kind::Property};
        std::string property;
        std::vector<std::string> values;
        std::vector<Condition> children;
    };
    struct MultipartPart {
        std::optional<Condition> when;
        std::vector<BlockModelVariant> apply;
    };
    std::vector<MultipartPart> multipart;
    bool isMultipart{};
    std::string error;
    explicit operator bool() const noexcept { return error.empty(); }
};

// Reads supplied Java model resources, including inheritance, blockstate
// variants/multipart selectors, and rotated elements.
class BlockModelLoader {
public:
    explicit BlockModelLoader(std::filesystem::path resourceRoot);
    ModelLoadResult loadModel(std::string_view identifier) const;
    BlockStateModels loadBlockState(std::string_view identifier) const;

private:
    std::filesystem::path root_;
};

} // namespace carbon
