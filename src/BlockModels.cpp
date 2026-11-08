#include "carbon/BlockModels.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <stdexcept>

namespace carbon {
namespace {
using Json = nlohmann::json;

std::string canonicalId(std::string_view raw) {
    if (raw.empty()) throw std::runtime_error("empty resource identifier");
    std::string result(raw);
    if (result.find(':') == std::string::npos) result = "minecraft:" + result;
    const auto colon = result.find(':');
    if (colon == 0 || colon + 1 == result.size() || result.find(':', colon + 1) != std::string::npos)
        throw std::runtime_error("invalid resource identifier: " + result);
    const auto ns = result.substr(0, colon);
    const auto path = result.substr(colon + 1);
    auto validNs = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'; };
    auto validPath = [&](char c) { return validNs(c) || c == '/' ; };
    if (!std::all_of(ns.begin(), ns.end(), validNs) || !std::all_of(path.begin(), path.end(), validPath))
        throw std::runtime_error("invalid resource identifier: " + result);
    std::istringstream pathParts(path);
    std::string part;
    while (std::getline(pathParts, part, '/'))
        if (part.empty() || part == "." || part == "..") throw std::runtime_error("unsafe resource path: " + result);
    return result;
}

std::filesystem::path resourceFile(const std::filesystem::path& root, std::string_view folder, const std::string& id) {
    const auto colon = id.find(':');
    return root / "assets" / id.substr(0, colon) / std::string(folder) /
        (std::string(id.substr(colon + 1)) + ".json");
}

Json readJson(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("resource not found: " + file.string());
    Json value;
    try { in >> value; }
    catch (const std::exception& e) { throw std::runtime_error("invalid JSON in " + file.string() + ": " + e.what()); }
    if (!value.is_object()) throw std::runtime_error("expected JSON object in " + file.string());
    return value;
}

float number(const Json& j, const std::string& what) {
    if (!j.is_number()) throw std::runtime_error("expected number for " + what);
    const double n = j.get<double>();
    if (!std::isfinite(n) || n < -16.0 || n > 32.0) throw std::runtime_error("out-of-range coordinate for " + what);
    return static_cast<float>(n);
}

float finiteNumber(const Json& j, const std::string& what) {
    if (!j.is_number()) throw std::runtime_error("expected number for " + what);
    const double n = j.get<double>();
    if (!std::isfinite(n)) throw std::runtime_error("expected finite number for " + what);
    return static_cast<float>(n);
}

template<size_t N> std::array<float, N> vector(const Json& j, const std::string& what) {
    if (!j.is_array() || j.size() != N) throw std::runtime_error("expected " + std::to_string(N) + " values for " + what);
    std::array<float, N> out{};
    for (size_t i = 0; i < N; ++i) out[i] = number(j[i], what);
    return out;
}

template<size_t N> std::array<float, N> rawVector(const Json& j, const std::string& what) {
    if (!j.is_array() || j.size() != N) throw std::runtime_error("expected " + std::to_string(N) + " values for " + what);
    std::array<float, N> out{};
    for (size_t i = 0; i < N; ++i) out[i] = finiteNumber(j[i], what);
    return out;
}

std::string resolveTexture(const std::string& raw,
                           const std::map<std::string, std::string, std::less<>>& textures,
                           std::set<std::string>& active) {
    if (raw.empty() || raw[0] != '#') return canonicalId(raw);
    const std::string slot = raw.substr(1);
    const auto it = textures.find(slot);
    if (it == textures.end()) throw std::runtime_error("unresolved texture slot: " + raw);
    if (!active.insert(slot).second) throw std::runtime_error("cyclic texture slot: " + raw);
    auto resolved = resolveTexture(it->second, textures, active);
    active.erase(slot);
    return resolved;
}

BlockModel parseModelChain(const std::filesystem::path& root, const std::string& id,
                           std::set<std::string>& active) {
    if (!active.insert(id).second) throw std::runtime_error("cyclic model parent: " + id);
    const auto file = resourceFile(root, "models", id);
    const Json json = readJson(file);
    BlockModel result;
    result.id = id;
    if (json.contains("parent")) {
        if (!json["parent"].is_string()) throw std::runtime_error("model parent must be a string: " + id);
        const auto parentId = canonicalId(json["parent"].get<std::string>());
        result.parent = parentId;
        result = parseModelChain(root, parentId, active);
        result.id = id;
        result.parent = parentId;
    }
    if (json.contains("textures")) {
        if (!json["textures"].is_object()) throw std::runtime_error("model textures must be an object: " + id);
        for (auto it = json["textures"].begin(); it != json["textures"].end(); ++it) {
            if (!it.value().is_string()) throw std::runtime_error("texture slot must be a string: " + id + "#" + it.key());
            result.textures[it.key()] = it.value().get<std::string>();
        }
    }
    if (json.contains("ambientocclusion")) {
        if (!json["ambientocclusion"].is_boolean()) throw std::runtime_error("ambientocclusion must be boolean: " + id);
        result.ambientOcclusion = json["ambientocclusion"].get<bool>();
    }
    if (json.contains("elements")) {
        if (!json["elements"].is_array()) throw std::runtime_error("model elements must be an array: " + id);
        for (const auto& elementJson : json["elements"]) {
            if (!elementJson.is_object() || !elementJson.contains("from") || !elementJson.contains("to") || !elementJson.contains("faces"))
                throw std::runtime_error("element requires from, to, and faces: " + id);
            ModelElement element;
            element.from = vector<3>(elementJson["from"], "element.from");
            element.to = vector<3>(elementJson["to"], "element.to");
            if (elementJson.contains("rotation")) {
                const auto& rotationJson=elementJson["rotation"];
                if (!rotationJson.is_object() || !rotationJson.contains("axis") || !rotationJson["axis"].is_string() ||
                    !rotationJson.contains("angle")) throw std::runtime_error("element rotation requires axis and angle: " + id);
                ModelElement::Rotation rotation;
                rotation.axis=rotationJson["axis"].get<std::string>();
                if (rotation.axis!="x" && rotation.axis!="y" && rotation.axis!="z")
                    throw std::runtime_error("element rotation axis must be x, y, or z: " + id);
                rotation.angle=finiteNumber(rotationJson["angle"],"element.rotation.angle");
                if (rotation.angle < -45.0f || rotation.angle > 45.0f)
                    throw std::runtime_error("element rotation angle must be between -45 and 45 degrees: " + id);
                if (rotationJson.contains("origin")) rotation.origin=rawVector<3>(rotationJson["origin"],"element.rotation.origin");
                if (rotationJson.contains("rescale")) {
                    if (!rotationJson["rescale"].is_boolean()) throw std::runtime_error("element rotation rescale must be boolean: " + id);
                    rotation.rescale=rotationJson["rescale"].get<bool>();
                }
                element.rotation=rotation;
            }
            if (!elementJson["faces"].is_object() || elementJson["faces"].empty()) throw std::runtime_error("element faces must be a non-empty object: " + id);
            for (auto faceIt = elementJson["faces"].begin(); faceIt != elementJson["faces"].end(); ++faceIt) {
                const Json& fj = faceIt.value();
                if (!fj.is_object() || !fj.contains("texture") || !fj["texture"].is_string())
                    throw std::runtime_error("face requires string texture: " + id);
                ModelFace face;
                face.texture = fj["texture"].get<std::string>(); // resolved after child slots are merged
                if (fj.contains("cullface")) {
                    if (!fj["cullface"].is_string()) throw std::runtime_error("cullface must be a string: " + id);
                    face.cullFace = fj["cullface"].get<std::string>();
                }
                if (fj.contains("tintindex")) {
                    if (!fj["tintindex"].is_number_integer()) throw std::runtime_error("tintindex must be an integer: " + id);
                    face.tintIndex = fj["tintindex"].get<int>();
                }
                if (fj.contains("uv")) face.uv = rawVector<4>(fj["uv"], "face.uv");
                if (fj.contains("rotation")) {
                    if (!fj["rotation"].is_number_integer()) throw std::runtime_error("face rotation must be an integer: " + id);
                    face.rotation = fj["rotation"].get<int>();
                    if (face.rotation % 90 != 0 || face.rotation < 0 || face.rotation > 270)
                        throw std::runtime_error("face rotation must be 0, 90, 180, or 270: " + id);
                }
                element.faces.emplace(faceIt.key(), std::move(face));
            }
            result.elements.push_back(std::move(element));
        }
    }
    if (json.contains("textures") && json["textures"].is_object() && json["textures"].contains("particle")) {
        if (!json["textures"]["particle"].is_string()) throw std::runtime_error("particle texture must be string: " + id);
        result.particle = json["textures"]["particle"].get<std::string>();
    }
    active.erase(id);
    return result;
}

std::map<std::string, std::string, std::less<>> parseSelector(const std::string& text) {
    std::map<std::string, std::string, std::less<>> result;
    if (text.empty()) return result;
    std::istringstream in(text);
    std::string field;
    while (std::getline(in, field, ',')) {
        const auto eq = field.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 == field.size()) throw std::runtime_error("invalid blockstate selector: " + text);
        if (!result.emplace(field.substr(0, eq), field.substr(eq + 1)).second) throw std::runtime_error("duplicate selector property: " + text);
    }
    return result;
}

int quadrant(const Json& j, std::string_view field) {
    int value = 0;
    if (!j.is_number_integer()) throw std::runtime_error(std::string(field) + " must be an integer");
    value = j.get<int>();
    if (value % 90 != 0 || value < 0 || value > 270) throw std::runtime_error(std::string(field) + " must be 0, 90, 180, or 270");
    return value;
}

BlockModelVariant parseVariant(const Json& j) {
    if (!j.is_object() || !j.contains("model") || !j["model"].is_string()) throw std::runtime_error("variant requires a string model");
    BlockModelVariant result;
    result.model = canonicalId(j["model"].get<std::string>());
    if (j.contains("x")) result.xRotation = quadrant(j["x"], "x rotation");
    if (j.contains("y")) result.yRotation = quadrant(j["y"], "y rotation");
    if (j.contains("z")) result.zRotation = quadrant(j["z"], "z rotation");
    if (j.contains("uvlock")) {
        if (!j["uvlock"].is_boolean()) throw std::runtime_error("uvlock must be boolean");
        result.uvLock = j["uvlock"].get<bool>();
    }
    if (j.contains("weight")) {
        if (!j["weight"].is_number_integer() || j["weight"].get<int>() <= 0) throw std::runtime_error("variant weight must be a positive integer");
        result.weight = j["weight"].get<int>();
    }
    return result;
}
} // namespace

BlockModelLoader::BlockModelLoader(std::filesystem::path resourceRoot) : root_(std::filesystem::canonical(std::move(resourceRoot))) {}

ModelLoadResult BlockModelLoader::loadModel(std::string_view identifier) const {
    try {
        const auto id = canonicalId(identifier);
        std::set<std::string> active;
        BlockModel model = parseModelChain(root_, id, active);
        // Java resolves texture slots across the complete parent chain after model wrapping.
        for (auto& element : model.elements) for (auto& [direction, face] : element.faces) {
            (void)direction;
            std::set<std::string> textureActive;
            face.texture = resolveTexture(face.texture, model.textures, textureActive);
        }
        if (model.particle) {
            std::set<std::string> textureActive;
            model.particle = resolveTexture(*model.particle, model.textures, textureActive);
        }
        for (auto& [slot, value] : model.textures) {
            (void)slot;
            if (!value.empty() && value[0] == '#') {
                std::set<std::string> textureActive;
                value = resolveTexture(value, model.textures, textureActive);
            } else value = canonicalId(value);
        }
        return {std::move(model), {}};
    } catch (const std::exception& e) { return {std::nullopt, e.what()}; }
}

BlockStateModels BlockModelLoader::loadBlockState(std::string_view identifier) const {
    BlockStateModels result;
    try {
        const auto id = canonicalId(identifier);
        const Json json = readJson(resourceFile(root_, "blockstates", id));
        if (json.contains("multipart")) {
            result.isMultipart=true;
            if (!json["multipart"].is_array() || json["multipart"].empty()) throw std::runtime_error("multipart must be a non-empty array");
            std::function<BlockStateModels::Condition(const Json&)> parseCondition;
            parseCondition=[&](const Json& value)->BlockStateModels::Condition {
                using Condition=BlockStateModels::Condition;
                if (!value.is_object() || value.empty()) throw std::runtime_error("multipart condition must be a non-empty object");
                Condition combined; combined.kind=Condition::Kind::All;
                for (auto it=value.begin();it!=value.end();++it) {
                    if (it.key()=="OR" || it.key()=="AND") {
                        if (!it.value().is_array() || it.value().empty()) throw std::runtime_error("multipart OR/AND condition must be a non-empty array");
                        Condition nested; nested.kind=it.key()=="OR"?Condition::Kind::Any:Condition::Kind::All;
                        for (const auto& child:it.value()) nested.children.push_back(parseCondition(child));
                        combined.children.push_back(std::move(nested));
                    } else {
                        if (it.key().empty()) throw std::runtime_error("multipart property condition key cannot be empty");
                        std::string rawValues;
                        if(it.value().is_string()) rawValues=it.value().get<std::string>();
                        else if(it.value().is_boolean()) rawValues=it.value().get<bool>()?"true":"false";
                        else if(it.value().is_number_integer()) rawValues=std::to_string(it.value().get<std::int64_t>());
                        else throw std::runtime_error("multipart property condition must be a string, boolean, or integer");
                        Condition property; property.kind=Condition::Kind::Property; property.property=it.key();
                        std::istringstream values(rawValues); std::string allowed;
                        while (std::getline(values,allowed,'|')) {
                            if (allowed.empty() || allowed=="!") throw std::runtime_error("multipart property values cannot be empty");
                            property.values.push_back(allowed);
                        }
                        if (property.values.empty()) throw std::runtime_error("multipart property condition has no values");
                        combined.children.push_back(std::move(property));
                    }
                }
                return combined;
            };
            for (const auto& partJson:json["multipart"]) {
                if (!partJson.is_object() || !partJson.contains("apply")) throw std::runtime_error("multipart part requires apply");
                BlockStateModels::MultipartPart part;
                if (partJson.contains("when")) part.when=parseCondition(partJson["when"]);
                const auto& apply=partJson["apply"];
                if (apply.is_array()) {
                    if (apply.empty()) throw std::runtime_error("multipart alternatives cannot be empty");
                    for (const auto& alternative:apply) part.apply.push_back(parseVariant(alternative));
                } else part.apply.push_back(parseVariant(apply));
                result.multipart.push_back(std::move(part));
            }
            if (json.contains("variants")) throw std::runtime_error("blockstate cannot contain both multipart and variants");
            return result;
        }
        if (!json.contains("variants") || !json["variants"].is_object() || json["variants"].empty())
            throw std::runtime_error("blockstate requires non-empty variants object");
        for (auto it = json["variants"].begin(); it != json["variants"].end(); ++it) {
            (void)parseSelector(it.key());
            std::vector<BlockModelVariant> variants;
            if (it.value().is_array()) {
                if (it.value().empty()) throw std::runtime_error("variant alternatives cannot be empty");
                for (const auto& v : it.value()) variants.push_back(parseVariant(v));
            } else variants.push_back(parseVariant(it.value()));
            result.variants.emplace(it.key(), std::move(variants));
        }
    } catch (const std::exception& e) { result.error = e.what(); }
    return result;
}

} // namespace carbon
