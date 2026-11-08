#pragma once

#include "carbon/Identifier.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace carbon {

enum class ResourceTree { assets, data };

// Read-only access to the supplied directory layout: assets/<namespace>/<path>
// and data/<namespace>/<path>. This does not implement packs, overlays, or the
// Java resource manager. Identifier validity and safe filesystem resolution
// are deliberately separate checks.
class DirectoryResources {
public:
    explicit DirectoryResources(std::filesystem::path sourceRoot);
    std::optional<std::vector<std::uint8_t>> read(ResourceTree tree, const Identifier& id) const;
    const std::filesystem::path& sourceRoot() const noexcept { return root_; }

private:
    std::filesystem::path root_;
};

} // namespace carbon
