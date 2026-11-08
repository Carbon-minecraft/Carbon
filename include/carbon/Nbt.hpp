#pragma once

// Value-model subset translated from Java Edition 26.3 net.minecraft.nbt.
// Lists retain the source API's permissive in-memory behavior (add/set do not
// check element IDs); binary NBT serialization and SNBT parsing are separate work.

#include <bit>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <limits>
#include <optional>
#include <string>
#include <stdexcept>
#include <variant>
#include <vector>
#include <type_traits>

namespace carbon::nbt {

enum class TagId : std::uint8_t {
    End = 0, Byte = 1, Short = 2, Int = 3, Long = 4, Float = 5,
    Double = 6, ByteArray = 7, String = 8, List = 9, Compound = 10,
    IntArray = 11, LongArray = 12
};

struct EndTag { constexpr bool operator==(const EndTag&) const = default; };
struct NumericTag {
    using Value = std::variant<std::int8_t, std::int16_t, std::int32_t,
                               std::int64_t, float, double>;
    Value value{std::int8_t{0}};
    bool operator==(const NumericTag& rhs) const noexcept {
        if (value.index() != rhs.value.index()) return false;
        if (const auto* a = std::get_if<float>(&value)) {
            const auto b = std::get<float>(rhs.value);
            const std::uint32_t ab = std::isnan(*a) ? 0x7fc00000u : std::bit_cast<std::uint32_t>(*a);
            const std::uint32_t bb = std::isnan(b) ? 0x7fc00000u : std::bit_cast<std::uint32_t>(b);
            return ab == bb;
        }
        if (const auto* a = std::get_if<double>(&value)) {
            const auto b = std::get<double>(rhs.value);
            const std::uint64_t ab = std::isnan(*a) ? 0x7ff8000000000000ull : std::bit_cast<std::uint64_t>(*a);
            const std::uint64_t bb = std::isnan(b) ? 0x7ff8000000000000ull : std::bit_cast<std::uint64_t>(b);
            return ab == bb;
        }
        return value == rhs.value;
    }

    TagId id() const noexcept { return static_cast<TagId>(value.index() + 1); }
    std::int8_t byteValue() const noexcept {
        return std::visit([](auto v) -> std::int8_t {
            using T = decltype(v);
            const auto low = [&]() -> std::uint8_t {
                if constexpr (std::is_integral_v<T>) return static_cast<std::uint8_t>(v);
                else return static_cast<std::uint8_t>(floorToInt(v));
            }();
            return std::bit_cast<std::int8_t>(low);
        }, value);
    }
    std::int16_t shortValue() const noexcept {
        return std::visit([](auto v) -> std::int16_t {
            using T = decltype(v);
            const auto low = [&]() -> std::uint16_t {
                if constexpr (std::is_integral_v<T>) return static_cast<std::uint16_t>(v);
                else return static_cast<std::uint16_t>(floorToInt(v));
            }();
            return std::bit_cast<std::int16_t>(low);
        }, value);
    }
    std::int32_t intValue() const noexcept {
        return std::visit([](auto v) -> std::int32_t {
            using T = decltype(v);
            if constexpr (std::is_integral_v<T>) return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(v));
            else return floorToInt(v);
        }, value);
    }
    std::int64_t longValue() const noexcept {
        return std::visit([](auto v) -> std::int64_t {
            using T = decltype(v);
            if constexpr (std::is_integral_v<T>) return static_cast<std::int64_t>(v);
            else if constexpr (std::is_same_v<T, float>) return javaCastToLong(v);
            else return floorToLong(v);
        }, value);
    }
    float floatValue() const noexcept { return std::visit([](auto v) { return static_cast<float>(v); }, value); }
    double doubleValue() const noexcept { return std::visit([](auto v) { return static_cast<double>(v); }, value); }

private:
    template<class T> static std::int64_t javaCastToLong(T v) noexcept {
        if (std::isnan(v)) return 0;
        if (v >= static_cast<T>(std::numeric_limits<std::int64_t>::max())) return std::numeric_limits<std::int64_t>::max();
        if (v <= static_cast<T>(std::numeric_limits<std::int64_t>::min())) return std::numeric_limits<std::int64_t>::min();
        return static_cast<std::int64_t>(v); // Java cast truncates toward zero.
    }
    static std::int64_t floorToLong(double v) noexcept {
        if (std::isnan(v)) return 0;
        if (v >= static_cast<double>(std::numeric_limits<std::int64_t>::max())) return std::numeric_limits<std::int64_t>::max();
        if (v <= static_cast<double>(std::numeric_limits<std::int64_t>::min())) return std::numeric_limits<std::int64_t>::min();
        return static_cast<std::int64_t>(std::floor(v));
    }
    static std::int64_t floorToLong(float v) noexcept { return floorToLong(static_cast<double>(v)); }
    static std::int32_t floorToInt(double v) noexcept {
        if (std::isnan(v)) return 0;
        if (v >= static_cast<double>(std::numeric_limits<std::int32_t>::max())) return std::numeric_limits<std::int32_t>::max();
        if (v <= static_cast<double>(std::numeric_limits<std::int32_t>::min())) return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(std::floor(v));
    }
    static std::int32_t floorToInt(float v) noexcept { return floorToInt(static_cast<double>(v)); }
};

class Tag;
class ListTag;
class CompoundTag;
using ByteArray = std::vector<std::int8_t>;
using IntArray = std::vector<std::int32_t>;
using LongArray = std::vector<std::int64_t>;
using Utf16String = std::u16string; // Java String is a sequence of UTF-16 code units.

class Tag {
public:
    using Value = std::variant<EndTag, NumericTag, ByteArray, Utf16String,
                               std::shared_ptr<ListTag>, std::shared_ptr<CompoundTag>,
                               IntArray, LongArray>;
    Tag() : value_(EndTag{}) {}
    explicit Tag(EndTag v) : value_(v) {}
    explicit Tag(NumericTag v) : value_(std::move(v)) {}
    explicit Tag(ByteArray v) : value_(std::move(v)) {}
    explicit Tag(Utf16String v) : value_(std::move(v)) {}
    explicit Tag(IntArray v) : value_(std::move(v)) {}
    explicit Tag(LongArray v) : value_(std::move(v)) {}
    explicit Tag(const ListTag& v);
    explicit Tag(const CompoundTag& v);

    static Tag byteTag(std::int8_t v) { return Tag(NumericTag{v}); }
    static Tag shortTag(std::int16_t v) { return Tag(NumericTag{v}); }
    static Tag intTag(std::int32_t v) { return Tag(NumericTag{v}); }
    static Tag longTag(std::int64_t v) { return Tag(NumericTag{v}); }
    static Tag floatTag(float v) { return Tag(NumericTag{v}); }
    static Tag doubleTag(double v) { return Tag(NumericTag{v}); }
    static Tag string(Utf16String v) { return Tag(std::move(v)); }

    TagId id() const noexcept {
        if (const auto* n = std::get_if<NumericTag>(&value_)) return n->id();
        switch (value_.index()) {
            case 0: return TagId::End;
            case 2: return TagId::ByteArray;
            case 3: return TagId::String;
            case 4: return TagId::List;
            case 5: return TagId::Compound;
            case 6: return TagId::IntArray;
            default: return TagId::LongArray;
        }
    }
    const NumericTag* numeric() const noexcept { return std::get_if<NumericTag>(&value_); }
    const Utf16String* stringValue() const noexcept { return std::get_if<Utf16String>(&value_); }
    ByteArray* byteArray() noexcept { return std::get_if<ByteArray>(&value_); }
    const ByteArray* byteArray() const noexcept { return std::get_if<ByteArray>(&value_); }
    IntArray* intArray() noexcept { return std::get_if<IntArray>(&value_); }
    const IntArray* intArray() const noexcept { return std::get_if<IntArray>(&value_); }
    LongArray* longArray() noexcept { return std::get_if<LongArray>(&value_); }
    const LongArray* longArray() const noexcept { return std::get_if<LongArray>(&value_); }
    ListTag* list() noexcept;
    const ListTag* list() const noexcept;
    CompoundTag* compound() noexcept;
    const CompoundTag* compound() const noexcept;
    Tag copy() const;
    bool operator==(const Tag& rhs) const;
private:
    Value value_;
};

class ListTag {
public:
    ListTag() = default;
    explicit ListTag(std::vector<Tag> values) : values_(std::move(values)) {}
    std::size_t size() const noexcept { return values_.size(); }
    bool empty() const noexcept { return values_.empty(); }
    const Tag* get(std::size_t i) const noexcept { return i < size() ? &values_[i] : nullptr; }
    Tag* get(std::size_t i) noexcept { return i < size() ? &values_[i] : nullptr; }
    void add(Tag value) { values_.push_back(std::move(value)); }
    void insert(std::size_t i, Tag value) {
        if (i > size()) throw std::out_of_range("ListTag index");
        values_.insert(values_.begin() + i, std::move(value));
    }
    Tag remove(std::size_t i) { Tag old = std::move(values_.at(i)); values_.erase(values_.begin() + i); return old; }
    void set(std::size_t i, Tag value) { values_.at(i) = std::move(value); }
    void clear() noexcept { values_.clear(); }
    TagId rawElementType() const noexcept {
        if (values_.empty()) return TagId::End;
        const TagId first = values_.front().id();
        for (const auto& value : values_) if (value.id() != first) return TagId::Compound;
        return first;
    }
    const std::vector<Tag>& values() const noexcept { return values_; }
    ListTag copy() const { ListTag out; for (const auto& t : values_) out.add(t.copy()); return out; }
    bool operator==(const ListTag& rhs) const { return values_ == rhs.values_; }
private:
    std::vector<Tag> values_;
};

class CompoundTag {
public:
    using Map = std::map<Utf16String, Tag>;
    Tag* get(const Utf16String& key) noexcept { auto it = tags_.find(key); return it == tags_.end() ? nullptr : &it->second; }
    const Tag* get(const Utf16String& key) const noexcept { auto it = tags_.find(key); return it == tags_.end() ? nullptr : &it->second; }
    std::optional<Tag> put(Utf16String key, Tag tag) {
        auto it = tags_.find(key);
        if (it == tags_.end()) { tags_.emplace(std::move(key), std::move(tag)); return {}; }
        Tag old = std::move(it->second); it->second = std::move(tag); return old;
    }
    bool contains(const Utf16String& key) const { return tags_.contains(key); }
    std::size_t size() const noexcept { return tags_.size(); }
    bool empty() const noexcept { return tags_.empty(); }
    const Map& entries() const noexcept { return tags_; }
    void putByte(const Utf16String& k, std::int8_t v) { put(k, Tag::byteTag(v)); }
    void putShort(const Utf16String& k, std::int16_t v) { put(k, Tag::shortTag(v)); }
    void putInt(const Utf16String& k, std::int32_t v) { put(k, Tag::intTag(v)); }
    void putLong(const Utf16String& k, std::int64_t v) { put(k, Tag::longTag(v)); }
    void putFloat(const Utf16String& k, float v) { put(k, Tag::floatTag(v)); }
    void putDouble(const Utf16String& k, double v) { put(k, Tag::doubleTag(v)); }
    std::optional<std::int8_t> getByte(const Utf16String& k) const { return number<std::int8_t>(k, &NumericTag::byteValue); }
    std::optional<std::int16_t> getShort(const Utf16String& k) const { return number<std::int16_t>(k, &NumericTag::shortValue); }
    std::optional<std::int32_t> getInt(const Utf16String& k) const { return number<std::int32_t>(k, &NumericTag::intValue); }
    std::optional<std::int64_t> getLong(const Utf16String& k) const { return number<std::int64_t>(k, &NumericTag::longValue); }
    std::optional<float> getFloat(const Utf16String& k) const { return number<float>(k, &NumericTag::floatValue); }
    std::optional<double> getDouble(const Utf16String& k) const { return number<double>(k, &NumericTag::doubleValue); }
    std::optional<bool> getBoolean(const Utf16String& k) const { auto n = getByte(k); return n ? std::optional<bool>(*n != 0) : std::nullopt; }
    std::optional<Utf16String> getString(const Utf16String& k) const {
        auto* t = get(k); auto* s = t ? t->stringValue() : nullptr; return s ? std::optional<Utf16String>(*s) : std::nullopt;
    }
    CompoundTag copy() const { CompoundTag out; for (const auto& [k,v] : tags_) out.put(k, v.copy()); return out; }
    bool operator==(const CompoundTag& rhs) const { return tags_ == rhs.tags_; }
private:
    template<class T> std::optional<T> number(const Utf16String& k, T (NumericTag::*cast)() const noexcept) const {
        auto* t = get(k); auto* n = t ? t->numeric() : nullptr; return n ? std::optional<T>((n->*cast)()) : std::nullopt;
    }
    Map tags_;
};

inline Tag::Tag(const ListTag& v) : value_(std::make_shared<ListTag>(v.copy())) {}
inline Tag::Tag(const CompoundTag& v) : value_(std::make_shared<CompoundTag>(v.copy())) {}
inline ListTag* Tag::list() noexcept { auto p = std::get_if<std::shared_ptr<ListTag>>(&value_); return p ? p->get() : nullptr; }
inline const ListTag* Tag::list() const noexcept { auto p = std::get_if<std::shared_ptr<ListTag>>(&value_); return p ? p->get() : nullptr; }
inline CompoundTag* Tag::compound() noexcept { auto p = std::get_if<std::shared_ptr<CompoundTag>>(&value_); return p ? p->get() : nullptr; }
inline const CompoundTag* Tag::compound() const noexcept { auto p = std::get_if<std::shared_ptr<CompoundTag>>(&value_); return p ? p->get() : nullptr; }
inline Tag Tag::copy() const {
    if (auto p = std::get_if<std::shared_ptr<ListTag>>(&value_)) return Tag(**p);
    if (auto p = std::get_if<std::shared_ptr<CompoundTag>>(&value_)) return Tag(**p);
    return *this;
}
inline bool Tag::operator==(const Tag& rhs) const {
    if (value_.index() != rhs.value_.index()) return false;
    if (auto p = std::get_if<std::shared_ptr<ListTag>>(&value_)) return **p == **std::get_if<std::shared_ptr<ListTag>>(&rhs.value_);
    if (auto p = std::get_if<std::shared_ptr<CompoundTag>>(&value_)) return **p == **std::get_if<std::shared_ptr<CompoundTag>>(&rhs.value_);
    return value_ == rhs.value_;
}

} // namespace carbon::nbt
