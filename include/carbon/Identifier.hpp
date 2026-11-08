#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace carbon {

// Identifier parsing follows the supplied net.minecraft.resources.Identifier
// tryParse/tryBySeparator rules. Filesystem safety is enforced separately by
// DirectoryResources, because Java permits some identifiers that are unsafe
// or unhelpful as direct paths (for example, paths containing ".." segments).
class Identifier {
public:
    static constexpr std::string_view defaultNamespace = "minecraft";

    static bool isValidNamespace(std::string_view value) noexcept;
    static bool isValidPath(std::string_view value) noexcept;
    static std::optional<Identifier> tryParse(std::string_view value);
    static std::optional<Identifier> tryBuild(std::string_view nameSpace, std::string_view path);

    const std::string& nameSpace() const noexcept { return namespace_; }
    const std::string& path() const noexcept { return path_; }
    std::string toString() const { return namespace_ + ":" + path_; }

    friend bool operator==(const Identifier&, const Identifier&) = default;

private:
    Identifier(std::string nameSpace, std::string path)
        : namespace_(std::move(nameSpace)), path_(std::move(path)) {}
    std::string namespace_;
    std::string path_;
};

} // namespace carbon
