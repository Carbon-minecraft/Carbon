#include "carbon/GuiItemRenderer.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
bool near(float a, float b) { return std::abs(a - b) < 1.0e-5f; }
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::filesystem::path root = argv[1];

    const auto stone = carbon::loadSourceGuiItemModel(root, "minecraft:stone");
    assert(stone);
    assert(stone.model == "minecraft:block/stone");
    // Source block/block.json is inherited by cube_all and the stone model.
    assert(near(stone.gui.rotationDegrees[0], 30.0f));
    assert(near(stone.gui.rotationDegrees[1], 225.0f));
    assert(near(stone.gui.scale[0], 0.625f));
    // Oracle for ItemTransform.apply's Quaternionf.rotationXYZ(30°,225°,0°):
    // qx*qy*qz acts on a column vector as Z, then Y, then X.
    const auto stoneCorner = carbon::transformGuiItemPoint({1.0f, 0.25f, 0.75f}, stone.gui);
    assert(near(stoneCorner[0], -0.3314563f));
    assert(near(stoneCorner[1], -0.1905592f));
    assert(near(stoneCorner[2], 0.0175582f));

    const auto paleOakFence = carbon::loadSourceGuiItemModel(root, "pale_oak_fence");
    assert(paleOakFence);
    assert(paleOakFence.model == "minecraft:block/pale_oak_fence_inventory");
    assert(near(paleOakFence.gui.rotationDegrees[0], 30.0f));
    assert(near(paleOakFence.gui.rotationDegrees[1], 135.0f));

    carbon::GuiItemTransform identity;
    identity.rotationDegrees = {0, 0, 0};
    identity.translation = {0, 0, 0};
    identity.scale = {1, 1, 1};
    const auto point = carbon::transformGuiItemPoint({1, 0.25f, 0.75f}, identity);
    assert(near(point[0], 0.5f) && near(point[1], -0.25f) && near(point[2], 0.25f));
    identity.rotationDegrees = {0, 90, 0};
    const auto rotated = carbon::transformGuiItemPoint({1, 0.5f, 0.5f}, identity);
    assert(near(rotated[0], 0.0f) && near(rotated[1], 0.0f) && near(rotated[2], -0.5f));
    identity.rotationDegrees = {0, 0, 0};
    identity.scale = {2, 1, 0.5f};
    const auto normal = carbon::transformGuiItemNormal({1, 1, 0}, identity);
    assert(near(normal[0], 0.5f) && near(normal[1], 1.0f) && near(normal[2], 0.0f));

    // 26.3 Window.calculateScale(0, false) minimums are 320x240 logical px.
    assert(carbon::sourceAutoGuiScale(319, 239) == 1);
    assert(carbon::sourceAutoGuiScale(640, 480) == 2);
    assert(carbon::sourceAutoGuiScale(1280, 720) == 3);
    assert(carbon::sourceAutoGuiScale(854, 480) == 2);
    assert(carbon::sourceAutoGuiScale(2549, 1527) == 6);
    assert(carbon::sourceAutoGuiScale(1280, 720, true) == 4);

    // Exercise odd/even framebuffer widths and non-divisible GUI scaling.
    for (const int width : {960, 961, 1280, 1281}) {
        for (const int scale : {1, 2, 3, 4}) {
            const auto layout = carbon::sourceHotbarLayout(width, 541, scale, 0);
            const int guiWidth = width / scale + (width % scale != 0);
            const int guiHeight = 541 / scale + (541 % scale != 0);
            const int center = guiWidth / 2;
            assert(layout.guiWidth == guiWidth && layout.guiHeight == guiHeight);
            assert(layout.screenCenter == center && layout.guiScale == scale);
            assert(layout.background.x == (center - 91) * scale);
            assert(layout.background.y == (guiHeight - 22) * scale);
            assert(layout.background.width == 182 * scale && layout.background.height == 22 * scale);
            assert(layout.selection.x == (center - 92) * scale);
            assert(layout.selection.y == (guiHeight - 23) * scale);
            assert(layout.selection.width == 24 * scale && layout.selection.height == 23 * scale);
            for (int slot = 0; slot < 9; ++slot) {
                const auto cell = layout.itemCells[static_cast<std::size_t>(slot)];
                assert(cell.x == (center - 88 + slot * 20) * scale);
                assert(cell.y == (guiHeight - 19) * scale);
                assert(cell.width == 16 * scale && cell.height == 16 * scale);
            }
            for (int selected = 0; selected < 9; ++selected) {
                const auto selectedLayout = carbon::sourceHotbarLayout(width, 541, scale, selected);
                assert(selectedLayout.selection.x == (center - 92 + selected * 20) * scale);
                assert(selectedLayout.selection.y == (guiHeight - 23) * scale);
            }
        }
    }

    std::cout << "PASS: native source item JSON and inherited GUI transforms\n";
}
