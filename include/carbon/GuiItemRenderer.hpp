#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace carbon {

struct GuiItemTransform {
    std::array<float, 3> rotationDegrees{30.0f, 225.0f, 0.0f};
    std::array<float, 3> translation{};
    std::array<float, 3> scale{0.625f, 0.625f, 0.625f};
};

struct GuiItemModel {
    std::string model;
    GuiItemTransform gui;
    std::string error;
    explicit operator bool() const noexcept { return error.empty() && !model.empty(); }
};

struct GuiPixelRect {
    int x{};
    int y{};
    int width{};
    int height{};
};

struct GuiHotbarLayout {
    int guiScale{1};
    int guiWidth{};
    int guiHeight{};
    int screenCenter{};
    GuiPixelRect background;
    GuiPixelRect selection;
    std::array<GuiPixelRect, 9> itemCells{};
};

// Mirrors Window.calculateScale(0, false) and setGuiScale() from Java 26.3.
// The auto scale is bounded by the source minimum logical resolution 320x240.
int sourceAutoGuiScale(int framebufferWidth, int framebufferHeight,
                       bool enforceUnicode = false);

// Mirrors Hud.extractItemHotbar() in logical GUI coordinates, then maps each
// integer GUI pixel to its exact framebuffer pixel block using guiScale.
GuiHotbarLayout sourceHotbarLayout(int framebufferWidth, int framebufferHeight,
                                   int guiScale, int selectedSlot);

// Resolves the current assets/minecraft/items/*.json model reference and the
// inherited models/<id>.json display.gui transform. Unsupported item-model
// dispatch forms are returned as errors rather than guessed.
GuiItemModel loadSourceGuiItemModel(const std::filesystem::path& resourceRoot,
                                    std::string_view itemId);

// Applies the source GUI display transform to a point in normalized model
// coordinates. Coordinates are centered on the model origin before rotation.
std::array<float, 3> transformGuiItemPoint(const std::array<float, 3>& point,
                                          const GuiItemTransform& transform);

// Applies the source ItemTransform normal matrix: inverse scale followed by
// the same rotationXYZ quaternion. The caller may normalize for lighting.
std::array<float, 3> transformGuiItemNormal(const std::array<float, 3>& normal,
                                           const GuiItemTransform& transform);

} // namespace carbon
