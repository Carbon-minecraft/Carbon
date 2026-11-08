#pragma once

#include "carbon/PlayableWorld.hpp"
#include "carbon/BlockModels.hpp"
#include "carbon/CreativeHotbar.hpp"
#include "carbon/GuiItemRenderer.hpp"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <filesystem>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <map>
#include <memory>

namespace carbon {

struct ClientHud {
    CreativeHotbar hotbar;
    std::string status;
};

struct MenuButton {
    std::string label;
    int x{}, y{}, width{200}, height{20};
    bool hovered{}, disabled{};
};

struct RendererStats {
    std::uint64_t frames{};
    std::uint64_t visibleBlocks{};
    std::uint64_t visibleQuads{};
    std::uint64_t culledQuads{};
    std::uint64_t meshBuilds{};
    std::uint64_t meshCacheHits{};
    std::uint64_t drawCalls{};
    std::uint64_t lightIndexBuilds{};
    double lastCpuFrameMs{};
    double lastLightIndexMs{}, lastSectionHashMs{}, lastMeshPrepareMs{}, lastDisplayListBuildMs{}, lastWorldDrawMs{};
    std::string glVendor, glRenderer, glVersion;
};

// A source model face compiled into local block coordinates. UV coordinates
// remain in Java's 0..16 model texture space until submitted to OpenGL.
struct SourceModelQuad {
    std::array<std::array<float, 3>, 4> positions{};
    std::array<std::array<float, 2>, 4> uvs{};
    std::array<float, 3> normal{};
    std::string texture;
    std::optional<std::string> cullFace;
    int tintIndex{-1};
};

struct SourceModelGeometry {
    std::vector<SourceModelQuad> quads;
    std::string error;
    explicit operator bool() const noexcept { return error.empty(); }
};

// Resolves a source blockstate variant (including weighted alternatives and
// blockstate rotations/uvlock) and bakes its model faces from Java resources.
SourceModelGeometry loadSourceBlockGeometry(const std::filesystem::path& resourceRoot,
                                            std::string_view blockStateId,
                                            std::string_view selector = {},
                                            std::int32_t blockX = 0,
                                            std::int32_t blockY = 0,
                                            std::int32_t blockZ = 0,
                                            std::shared_ptr<const SourceModelGeometry>* cached = nullptr);

// Bakes a directly named source model (used by item inventory models) with
// its model element rotations and no blockstate rotation.
SourceModelGeometry loadSourceModelGeometry(const std::filesystem::path& resourceRoot,
                                           std::string_view modelId);

// Native WGL renderer for the playable client. Textures and model mappings are
// resolved directly from JavaMinecraft/decompiled/assets/minecraft.
class ClientRenderer {
public:
    ClientRenderer();
    ~ClientRenderer();
    ClientRenderer(const ClientRenderer&) = delete;
    ClientRenderer& operator=(const ClientRenderer&) = delete;

    bool initialize(HWND window, int width, int height,
                    std::filesystem::path sourceRoot,
                    std::string* error = nullptr);
    void resize(int width, int height);
    void render(std::span<const BlockInstance> blocks, const PlayerState& player,
                const ClientHud& hud = {}, std::uint64_t sceneRevision = 0,
                std::uint64_t snapshotToken = 0);
    void renderMenu(std::string_view title, std::span<const MenuButton> buttons,
                    bool titleScreen = false, std::string_view subtitle = {});
    void shutdown();
    bool ready() const noexcept { return context_ != nullptr; }
    std::uint64_t presentedFrames() const noexcept { return presentedFrames_; }
    std::size_t loadedTextureCount() const noexcept { return textures_.size(); }
    const std::string& lastError() const noexcept { return lastError_; }
    const RendererStats& performanceStats() const noexcept { return stats_; }
    bool writeScreenshot(const std::filesystem::path& path, std::string* error = nullptr);

private:
    HWND window_{};
    HDC dc_{};
    HGLRC context_{};
    bool ownsComInitialization_{};
    int width_{1}, height_{1};
    std::filesystem::path textureRoot_;
    std::filesystem::path resourceRoot_;
    std::unordered_map<std::string, unsigned int> textures_;
    std::map<std::string, SourceModelGeometry, std::less<>> sourceModels_;
    struct SectionMesh { std::uint64_t fingerprint{}; int x{},y{},z{}; std::map<std::string, unsigned int, std::less<>> textureLists; };
    std::unordered_map<std::int64_t, SectionMesh> sectionMeshes_;
    std::unique_ptr<class SmoothLightNeighborhood> lightingNeighborhood_;
    std::uint64_t cachedSceneFingerprint_{};
    std::uint64_t cachedSceneRevision_{};
    RendererStats stats_{};
    std::map<std::string, GuiItemModel, std::less<>> guiItemModels_;
    std::array<std::uint8_t, 128> glyphWidths_{};
    float grassTintR_{}, grassTintG_{}, grassTintB_{};
    bool loggedContextFailure_{}, loggedSwapFailure_{}, loggedGlFailure_{};
    std::uint64_t presentedFrames_{};
    float panoramaRotation_{};
    std::string lastError_;
};

} // namespace carbon


