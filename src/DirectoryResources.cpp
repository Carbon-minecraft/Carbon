#include "carbon/DirectoryResources.hpp"

#include <fstream>
#include <iterator>
#include <string>

namespace carbon {
namespace fs = std::filesystem;
namespace {
bool isWithin(const fs::path& child, const fs::path& root) {
    auto childPart = child.begin();
    auto rootPart = root.begin();
    for (; rootPart != root.end(); ++rootPart, ++childPart) {
        if (childPart == child.end() || *childPart != *rootPart) return false;
    }
    return true;
}
}

DirectoryResources::DirectoryResources(fs::path sourceRoot) {
    root_ = fs::canonical(std::move(sourceRoot));
}

std::optional<std::vector<std::uint8_t>> DirectoryResources::read(ResourceTree tree, const Identifier& id) const {
    const fs::path treePath = tree == ResourceTree::assets ? "assets" : "data";
    const fs::path base = root_ / treePath;
    std::error_code error;
    const fs::path canonicalBase = fs::canonical(base, error);
    if (error || !isWithin(canonicalBase, root_) || !fs::is_directory(canonicalBase, error) || error)
        return std::nullopt;

    // Identifier's character rules exclude native separators and drive syntax,
    // but '.' and '..' are valid path characters. Normalize and then verify
    // both lexical and canonical containment (the latter blocks symlink escape).
    const fs::path candidate = (canonicalBase / id.nameSpace() / fs::path(id.path())).lexically_normal();
    if (!isWithin(candidate, canonicalBase)) return std::nullopt;
    const fs::path canonicalFile = fs::canonical(candidate, error);
    if (error || !isWithin(canonicalFile, canonicalBase) || !fs::is_regular_file(canonicalFile, error) || error)
        return std::nullopt;

    std::ifstream input(canonicalFile, std::ios::binary);
    if (!input) return std::nullopt;
    std::vector<std::uint8_t> bytes;
    for (std::istreambuf_iterator<char> it(input), end; it != end; ++it)
        bytes.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(*it)));
    if (input.bad()) return std::nullopt;
    return bytes;
}

} // namespace carbon
