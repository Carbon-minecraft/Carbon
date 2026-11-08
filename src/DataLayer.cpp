#include "carbon/DataLayer.hpp"

#include <bit>
#include <charconv>
#include <stdexcept>
#include <utility>

namespace carbon {
namespace {
void appendJavaHex(std::string& out, std::int32_t value) {
    char buffer[8];
    const auto unsignedValue = static_cast<std::uint32_t>(value);
    const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), unsignedValue, 16);
    (void)error;
    out.append(buffer, end);
}
}

DataLayer::DataLayer(std::int32_t defaultValue) : defaultValue_(defaultValue) {}

DataLayer::DataLayer(std::vector<std::uint8_t> data) : data_(std::move(data)), materialized_(true) {
    if (data_.size() != SIZE) throw std::invalid_argument("DataLayer should be 2048 bytes not: " + std::to_string(data_.size()));
}

std::int32_t DataLayer::index(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
    // Java int shifts/ORs wrap to 32 bits, which is also defined for uint32_t.
    const auto ux = static_cast<std::uint32_t>(x);
    const auto uy = static_cast<std::uint32_t>(y);
    const auto uz = static_cast<std::uint32_t>(z);
    return std::bit_cast<std::int32_t>((uy << 8) | (uz << 4) | ux);
}

std::int32_t DataLayer::get(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return getAt(index(x, y, z));
}

std::int32_t DataLayer::getAt(std::int32_t at) const {
    if (!materialized_) return defaultValue_;
    const auto byteIndex = at >> 1;
    if (byteIndex < 0 || static_cast<std::size_t>(byteIndex) >= data_.size()) throw std::out_of_range("DataLayer index");
    return (data_[static_cast<std::size_t>(byteIndex)] >> (4 * (at & 1))) & 15;
}

void DataLayer::set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t value) {
    setAt(index(x, y, z), value);
}

void DataLayer::setAt(std::int32_t at, std::int32_t value) {
    auto& bytes = getData();
    const auto byteIndex = at >> 1;
    if (byteIndex < 0 || static_cast<std::size_t>(byteIndex) >= bytes.size()) throw std::out_of_range("DataLayer index");
    const auto shift = 4 * (at & 1);
    const auto mask = static_cast<std::uint8_t>(~(15u << shift));
    const auto nibble = static_cast<std::uint8_t>((static_cast<std::uint32_t>(value) & 15u) << shift);
    bytes[static_cast<std::size_t>(byteIndex)] = static_cast<std::uint8_t>((bytes[static_cast<std::size_t>(byteIndex)] & mask) | nibble);
}

void DataLayer::fill(std::int32_t value) {
    defaultValue_ = value;
    data_.clear();
    materialized_ = false;
}

std::vector<std::uint8_t>& DataLayer::getData() {
    if (!materialized_) {
        // Preserve Java packFilled's unusual whole-byte behavior for values
        // outside 0..15: byte(value) | byte(value << 4).
        const auto packed = static_cast<std::uint8_t>(static_cast<std::uint8_t>(defaultValue_) |
            static_cast<std::uint8_t>(static_cast<std::uint32_t>(defaultValue_) << 4));
        data_.assign(SIZE, packed);
        materialized_ = true;
    }
    return data_;
}

const std::vector<std::uint8_t>* DataLayer::dataIfMaterialized() const noexcept {
    return materialized_ ? &data_ : nullptr;
}

DataLayer DataLayer::copy() const {
    return materialized_ ? DataLayer(data_) : DataLayer(defaultValue_);
}

bool DataLayer::isDefinitelyHomogenous() const noexcept { return !materialized_; }
bool DataLayer::isDefinitelyFilledWith(std::int32_t value) const noexcept { return !materialized_ && defaultValue_ == value; }
bool DataLayer::isEmpty() const noexcept { return !materialized_ && defaultValue_ == 0; }

std::string DataLayer::toString() const {
    std::string out;
    out.reserve(4096 + 272);
    for (std::int32_t i = 0; i < 4096; ++i) {
        appendJavaHex(out, getAt(i));
        if ((i & 15) == 15) out.push_back('\n');
        if ((i & 0xff) == 0xff) out.push_back('\n');
    }
    return out;
}

std::string DataLayer::layerToString(std::int32_t layer) const {
    std::string out;
    out.reserve(272);
    // Java ignores the layer parameter and reads indices 0..255.
    (void)layer;
    for (std::int32_t i = 0; i < 256; ++i) {
        appendJavaHex(out, getAt(i));
        if ((i & 15) == 15) out.push_back('\n');
    }
    return out;
}

}
