#include "carbon/DirectoryResources.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

static std::string readText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const fs::path root = fs::absolute(argv[1]);

    // These are the supplied Java Identifier character and separator rules.
    assert(carbon::Identifier::tryParse("textures/block/stone.png")->toString() == "minecraft:textures/block/stone.png");
    assert(carbon::Identifier::tryParse(":foo")->toString() == "minecraft:foo");
    assert(carbon::Identifier::tryParse("mod:item")->toString() == "mod:item");
    assert(carbon::Identifier::tryParse("minecraft:").has_value()); // empty path is Java-valid
    assert(carbon::Identifier::tryBuild("", "")->toString() == ":"); // empty namespace/path are Java-valid
    assert(!carbon::Identifier::tryParse("Minecraft:item"));
    assert(!carbon::Identifier::tryParse("mod:two:colons"));
    assert(carbon::Identifier::tryParse("../outside")); // '..' is valid Java path text
    assert(!carbon::Identifier::tryParse("mod\\outside"));
    assert(!carbon::Identifier::tryBuild("..", "file"));
    assert(carbon::Identifier::tryBuild("mod", "a/../b")); // valid Identifier; access policy is separate

    carbon::DirectoryResources resources(root);
    const auto assetId = carbon::Identifier::tryParse("minecraft:waypoint_style/default.json");
    const auto dataId = carbon::Identifier::tryParse("minecraft:zombie_nautilus_variant/warm.json");
    assert(assetId && dataId);
    const auto asset = resources.read(carbon::ResourceTree::assets, *assetId);
    const auto data = resources.read(carbon::ResourceTree::data, *dataId);
    assert(asset && std::string(asset->begin(), asset->end()) == readText(root / "assets/minecraft/waypoint_style/default.json"));
    assert(data && std::string(data->begin(), data->end()) == readText(root / "data/minecraft/zombie_nautilus_variant/warm.json"));

    const auto emptyPath = carbon::Identifier::tryParse("minecraft:");
    assert(emptyPath && !resources.read(carbon::ResourceTree::assets, *emptyPath));
    assert(!resources.read(carbon::ResourceTree::assets, *carbon::Identifier::tryParse("../outside")));
    assert(!resources.read(carbon::ResourceTree::assets, *carbon::Identifier::tryParse("minecraft:../version.json")));
    assert(!resources.read(carbon::ResourceTree::assets, *carbon::Identifier::tryParse("minecraft:waypoint_style/../../version.json")));

    // Exercise canonical containment against a symlink where the host permits
    // creating one (Windows may require developer mode or elevated privileges).
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temp = fs::temp_directory_path() / ("carbon-resource-containment-test-" + std::to_string(nonce));
    fs::create_directories(temp / "root/assets/minecraft");
    fs::create_directories(temp / "outside");
    { std::ofstream out(temp / "outside/secret.json", std::ios::binary); out << "secret"; }
    std::error_code linkError;
    fs::create_directory_symlink(temp / "outside", temp / "root/assets/minecraft/leak", linkError);
    if (!linkError) {
        carbon::DirectoryResources isolated(temp / "root");
        const auto leaked = carbon::Identifier::tryParse("minecraft:leak/secret.json");
        assert(leaked && !isolated.read(carbon::ResourceTree::assets, *leaked));
    }
    fs::remove_all(temp);
}
