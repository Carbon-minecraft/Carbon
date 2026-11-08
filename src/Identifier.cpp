#include "carbon/Identifier.hpp"

namespace carbon {
namespace {
bool namespaceCharacter(char c) noexcept {
    return c == '_' || c == '-' || c == '.' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}
bool pathCharacter(char c) noexcept {
    return namespaceCharacter(c) || c == '/';
}
}

bool Identifier::isValidNamespace(std::string_view value) noexcept {
    if (value == "..") return false;
    for (char c : value) if (!namespaceCharacter(c)) return false;
    return true;
}

bool Identifier::isValidPath(std::string_view value) noexcept {
    for (char c : value) if (!pathCharacter(c)) return false;
    return true;
}

std::optional<Identifier> Identifier::tryBuild(std::string_view nameSpace, std::string_view path) {
    if (!isValidNamespace(nameSpace) || !isValidPath(path)) return std::nullopt;
    return Identifier(std::string(nameSpace), std::string(path));
}

std::optional<Identifier> Identifier::tryParse(std::string_view value) {
    const auto separator = value.find(':');
    if (separator == std::string_view::npos) {
        if (!isValidPath(value)) return std::nullopt;
        return Identifier(std::string(defaultNamespace), std::string(value));
    }

    const auto path = value.substr(separator + 1);
    if (!isValidPath(path)) return std::nullopt;
    if (separator == 0) return Identifier(std::string(defaultNamespace), std::string(path));

    const auto nameSpace = value.substr(0, separator);
    if (!isValidNamespace(nameSpace)) return std::nullopt;
    return Identifier(std::string(nameSpace), std::string(path));
}

} // namespace carbon
