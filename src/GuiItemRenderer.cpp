#include "carbon/GuiItemRenderer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace carbon {
namespace {
using Json = nlohmann::json;

std::string canonical(std::string value) {
    if (value.find(':') == std::string::npos) value = "minecraft:" + value;
    const auto colon = value.find(':');
    if (colon == 0 || colon + 1 == value.size() || value.find(':', colon + 1) != std::string::npos)
        throw std::runtime_error("invalid resource identifier: " + value);
    const auto path = value.substr(colon + 1);
    if (path.find("..") != std::string::npos || path.front() == '/' || path.back() == '/')
        throw std::runtime_error("unsafe resource identifier: " + value);
    return value;
}

std::filesystem::path fileFor(const std::filesystem::path& root, std::string_view folder,
                              const std::string& id) {
    const auto colon = id.find(':');
    return root / "assets" / id.substr(0, colon) / std::string(folder) /
           (id.substr(colon + 1) + ".json");
}

Json readObject(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("resource not found: " + path.string());
    Json value;
    try { in >> value; }
    catch (const std::exception& e) { throw std::runtime_error("invalid JSON in " + path.string() + ": " + e.what()); }
    if (!value.is_object()) throw std::runtime_error("expected JSON object in " + path.string());
    return value;
}

std::array<float, 3> vec3(const Json& value, const char* name) {
    if (!value.is_array() || value.size() != 3) throw std::runtime_error(std::string(name) + " must contain 3 numbers");
    std::array<float, 3> result{};
    for (std::size_t i = 0; i < 3; ++i) {
        if (!value[i].is_number()) throw std::runtime_error(std::string(name) + " must contain 3 numbers");
        const double n = value[i].get<double>();
        if (!std::isfinite(n) || std::abs(n) > 3600.0) throw std::runtime_error(std::string(name) + " has invalid number");
        result[i] = static_cast<float>(n);
    }
    return result;
}

void applyGui(const Json& object, GuiItemTransform& transform) {
    if (!object.is_object()) throw std::runtime_error("display.gui must be an object");
    if (object.contains("rotation")) transform.rotationDegrees = vec3(object["rotation"], "display.gui.rotation");
    if (object.contains("translation")) {
        transform.translation = vec3(object["translation"], "display.gui.translation");
        // Java ItemTransform.Deserializer converts translation from sixteenths
        // to model units, then clamps each axis to [-5, 5]. Keep JSON units in
        // this structure; transformGuiItemPoint divides them by sixteen.
        for (float& axis : transform.translation) axis = std::clamp(axis, -80.0f, 80.0f);
    }
    if (object.contains("scale")) {
        transform.scale = vec3(object["scale"], "display.gui.scale");
        for (float& axis : transform.scale) axis = std::clamp(axis, -4.0f, 4.0f);
    }
}

void readInheritedGui(const std::filesystem::path& root, const std::string& id,
                      GuiItemTransform& transform, std::set<std::string>& active) {
    if (!active.insert(id).second) throw std::runtime_error("cyclic model parent: " + id);
    const Json model = readObject(fileFor(root, "models", id));
    if (model.contains("parent")) {
        if (!model["parent"].is_string()) throw std::runtime_error("model parent must be a string: " + id);
        readInheritedGui(root, canonical(model["parent"].get<std::string>()), transform, active);
    }
    if (model.contains("display")) {
        if (!model["display"].is_object()) throw std::runtime_error("display must be an object: " + id);
        if (model["display"].contains("gui")) applyGui(model["display"]["gui"], transform);
    }
    active.erase(id);
}

std::array<float, 3> rotateGuiVector(const std::array<float, 3>& vector,
                                    const GuiItemTransform& transform) {
    constexpr float radians = 3.14159265358979323846f / 180.0f;
    const float rx = transform.rotationDegrees[0] * radians;
    const float ry = transform.rotationDegrees[1] * radians;
    const float rz = transform.rotationDegrees[2] * radians;
    const float cx = std::cos(rx), sx = std::sin(rx);
    const float cy = std::cos(ry), sy = std::sin(ry);
    const float cz = std::cos(rz), sz = std::sin(rz);
    float x = vector[0], y = vector[1], z = vector[2];
    // JOML rotationXYZ creates qx*qy*qz; column-vector points see Z, Y, X.
    const float x1 = cz * x - sz * y, y1 = sz * x + cz * y;
    x = x1; y = y1;
    const float x2 = cy * x + sy * z, z2 = -sy * x + cy * z;
    x = x2; z = z2;
    const float y2 = cx * y - sx * z, z3 = sx * y + cx * z;
    return {x, y2, z3};
}
} // namespace

int sourceAutoGuiScale(int framebufferWidth, int framebufferHeight, bool enforceUnicode) {
    if (framebufferWidth < 1 || framebufferHeight < 1)
        throw std::invalid_argument("framebuffer dimensions must be positive");
    int scale = 1;
    while (scale < framebufferWidth && scale < framebufferHeight &&
           framebufferWidth / (scale + 1) >= 320 &&
           framebufferHeight / (scale + 1) >= 240) {
        ++scale;
    }
    if (enforceUnicode && scale % 2 != 0) ++scale;
    return scale;
}

GuiHotbarLayout sourceHotbarLayout(int framebufferWidth, int framebufferHeight,
                                   int guiScale, int selectedSlot) {
    if (framebufferWidth < 1 || framebufferHeight < 1)
        throw std::invalid_argument("framebuffer dimensions must be positive");
    if (guiScale < 1) throw std::invalid_argument("GUI scale must be positive");
    if (selectedSlot < 0 || selectedSlot >= 9)
        throw std::invalid_argument("selected hotbar slot must be in [0, 8]");

    // Window.setGuiScale uses ceil(framebufferDimension / guiScale).
    GuiHotbarLayout result;
    result.guiScale = guiScale;
    result.guiWidth = framebufferWidth / guiScale + (framebufferWidth % guiScale != 0);
    result.guiHeight = framebufferHeight / guiScale + (framebufferHeight % guiScale != 0);
    result.screenCenter = result.guiWidth / 2;
    const auto pixelRect = [guiScale](int x, int y, int width, int height) {
        return GuiPixelRect{x * guiScale, y * guiScale,
                            width * guiScale, height * guiScale};
    };

    // Hud.extractItemHotbar: (center - 91, height - 22), 182x22.
    result.background = pixelRect(result.screenCenter - 91, result.guiHeight - 22, 182, 22);
    // HUD/hotbar_selection sprite: (center - 91 - 1 + selected*20, height-22-1), 24x23.
    result.selection = pixelRect(result.screenCenter - 92 + selectedSlot * 20,
                                 result.guiHeight - 23, 24, 23);
    for (int slot = 0; slot < 9; ++slot) {
        // Hud extracts items at center-90 + slot*20 + 2, height-16-3;
        // GuiRenderer blits each atlas item into a 16x16 GUI cell.
        result.itemCells[static_cast<std::size_t>(slot)] =
            pixelRect(result.screenCenter - 88 + slot * 20,
                      result.guiHeight - 19, 16, 16);
    }
    return result;
}

GuiItemModel loadSourceGuiItemModel(const std::filesystem::path& resourceRoot,
                                    std::string_view itemId) {
    GuiItemModel result;
    try {
        const std::string item = canonical(std::string(itemId));
        const Json json = readObject(fileFor(resourceRoot, "items", item));
        if (!json.contains("model") || !json["model"].is_object())
            throw std::runtime_error("item requires a model object: " + item);
        const Json& model = json["model"];
        if (!model.contains("type") || !model["type"].is_string() || model["type"].get<std::string>() != "minecraft:model")
            throw std::runtime_error("unsupported item model type for " + item);
        if (!model.contains("model") || !model["model"].is_string())
            throw std::runtime_error("item model requires a string model id: " + item);
        result.model = canonical(model["model"].get<std::string>());
        std::set<std::string> active;
        readInheritedGui(resourceRoot, result.model, result.gui, active);
    } catch (const std::exception& error) {
        result.model.clear();
        result.error = error.what();
    }
    return result;
}

std::array<float, 3> transformGuiItemPoint(const std::array<float, 3>& point,
                                          const GuiItemTransform& transform) {
    const auto rotated = rotateGuiVector({point[0] - 0.5f, point[1] - 0.5f,
                                          point[2] - 0.5f}, transform);
    return {rotated[0] * transform.scale[0] + transform.translation[0] / 16.0f,
            rotated[1] * transform.scale[1] + transform.translation[1] / 16.0f,
            rotated[2] * transform.scale[2] + transform.translation[2] / 16.0f};
}

std::array<float, 3> transformGuiItemNormal(const std::array<float, 3>& normal,
                                           const GuiItemTransform& transform) {
    std::array<float, 3> inverseScaled{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (transform.scale[axis] == 0.0f) return {0.0f, 0.0f, 0.0f};
        inverseScaled[axis] = normal[axis] / transform.scale[axis];
    }
    return rotateGuiVector(inverseScaled, transform);
}

} // namespace carbon
