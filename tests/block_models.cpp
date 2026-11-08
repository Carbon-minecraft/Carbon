#include "carbon/BlockModels.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void write(const fs::path& path, const char* text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << text;
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const fs::path resources = argv[1];
    carbon::BlockModelLoader loader(resources);

    // These are source resources from JavaMinecraft/decompiled. Check decoded
    // fields against their literal JSON semantics and inherited source parents.
    const auto cube = loader.loadModel("minecraft:block/stone");
    if (!cube) std::cerr << "stone: " << cube.error << '\n';
    assert(cube && cube.model->parent == "minecraft:block/cube_all");
    assert(cube.model->elements.size() == 1);
    const auto& cubeElement = cube.model->elements.front();
    assert((cubeElement.from == std::array<float, 3>{0, 0, 0}));
    assert((cubeElement.to == std::array<float, 3>{16, 16, 16}));
    assert(cubeElement.faces.size() == 6);
    for (const auto& [direction, face] : cubeElement.faces) {
        (void)direction;
        assert(face.texture == "minecraft:block/stone");
    }
    assert(cube.model->textures.at("all") == "minecraft:block/stone");

    const auto log = loader.loadModel("minecraft:block/oak_log");
    if (!log) std::cerr << "log: " << log.error << '\n';
    assert(log && log.model->elements.size() == 1);
    assert(log.model->elements[0].faces.at("up").texture == "minecraft:block/oak_log_top");
    assert(log.model->elements[0].faces.at("north").texture == "minecraft:block/oak_log");

    const auto state = loader.loadBlockState("minecraft:oak_log");
    if (!state) std::cerr << "oak_log state: " << state.error << '\n';
    assert(state);
    assert(state.variants.contains("axis=y"));
    const auto& yVariants = state.variants.at("axis=y");
    assert(yVariants.size() == 1 && yVariants[0].model == "minecraft:block/oak_log");
    assert(yVariants[0].xRotation == 0 && yVariants[0].yRotation == 0);

    const auto stoneState = loader.loadBlockState("minecraft:stone");
    assert(stoneState && stoneState.variants.at("").size() == 4);
    assert(stoneState.variants.at("")[2].yRotation == 180);

    const auto rotated = loader.loadBlockState("minecraft:oak_stairs");
    assert(rotated && rotated.variants.contains("facing=east,half=bottom,shape=straight"));
    const auto& rotatedPart = rotated.variants.at("facing=east,half=bottom,shape=inner_left").front();
    assert(rotatedPart.yRotation == 270 && rotatedPart.uvLock);

    const fs::path temp = fs::temp_directory_path() / "carbon-block-model-fixtures";
    fs::remove_all(temp);
    write(temp / "assets/minecraft/models/block/cycle_a.json", R"({"parent":"minecraft:block/cycle_b"})");
    write(temp / "assets/minecraft/models/block/cycle_b.json", R"({"parent":"minecraft:block/cycle_a"})");
    write(temp / "assets/minecraft/models/block/texture_cycle.json", R"({"textures":{"a":"#b","b":"#a"},"elements":[{"from":[0,0,0],"to":[16,16,16],"faces":{"up":{"texture":"#a"}}}]})");
    write(temp / "assets/minecraft/blockstates/multipart_only.json", R"({"multipart":[{"apply":{"model":"minecraft:block/cube_all"}}]})");
    carbon::BlockModelLoader fixtureLoader(temp);
    const auto cycle = fixtureLoader.loadModel("minecraft:block/cycle_a");
    assert(!cycle && cycle.error.find("cyclic model parent") != std::string::npos);
    const auto textureCycle = fixtureLoader.loadModel("minecraft:block/texture_cycle");
    assert(!textureCycle && textureCycle.error.find("cyclic texture slot") != std::string::npos);
    const auto multipart = fixtureLoader.loadBlockState("minecraft:multipart_only");
    assert(multipart && multipart.isMultipart && multipart.multipart.size()==1);
    const auto fence = loader.loadBlockState("minecraft:oak_fence");
    assert(fence && fence.isMultipart && fence.multipart.size()==5);
    fs::remove_all(temp);

    std::cout << "PASS: source block model inheritance, texture slots, variants and failure reporting\n";
}
