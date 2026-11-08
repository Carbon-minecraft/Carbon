#pragma once

// Source-backed palette subset from Minecraft Java 26.3. Registry values are
// object references in Java; use pointer/reference handles (typically
// std::shared_ptr<T>) here so identity is preserved instead of comparing the
// referenced object's value.

#include "carbon/BinaryIO.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace carbon {

template<class T>
class IdMap {
public:
    virtual ~IdMap() = default;
    virtual int getId(const T& value) const = 0;
    virtual T byId(int id) const = 0; // Null handle means missing, as in Java.
    virtual int size() const = 0;
    T byIdOrThrow(int id) const {
        T value = byId(id);
        if (!value) throw std::invalid_argument("No value with id " + std::to_string(id));
        return value;
    }
    int getIdOrThrow(const T& value) const {
        const int id = getId(value);
        if (id == -1) throw std::invalid_argument("Can't find id for value in map");
        return id;
    }
};

class MissingPaletteEntryException : public std::runtime_error {
public:
    explicit MissingPaletteEntryException(int index)
        : std::runtime_error("Missing Palette entry for index " + std::to_string(index) + ".") {}
};

template<class T>
class PaletteResize {
public:
    virtual ~PaletteResize() = default;
    virtual int onResize(int bits, const T& lastAddedValue) = 0;
};

template<class T, class Predicate = std::function<bool(const T&)>>
class Palette {
public:
    virtual ~Palette() = default;
    virtual int idFor(const T& value, PaletteResize<T>& resizeHandler) = 0;
    virtual bool maybeHas(Predicate predicate) const = 0;
    virtual T valueFor(int index) const = 0;
    virtual void read(ByteReader& buffer, const IdMap<T>& globalMap) = 0;
    virtual void write(ByteWriter& buffer, const IdMap<T>& globalMap) const = 0;
    virtual int getSerializedSize(const IdMap<T>& globalMap) const = 0;
    virtual int getSize() const = 0;
    virtual std::unique_ptr<Palette<T, Predicate>> copy() const = 0;
};

// Pointer and smart-pointer handles compare the referent address, matching
// Java's ==. Other types use their normal equality; callers porting Java
// objects should therefore use an identity handle, not a value type.
template<class T>
bool paletteSameObject(const T& a, const T& b) {
    if constexpr (std::is_pointer_v<T>) return a == b;
    else if constexpr (requires { a.get(); }) return a.get() == b.get();
    else return a == b;
}

inline int varIntSize(int value) {
    unsigned int bits = static_cast<unsigned int>(value);
    int count = 1;
    while ((bits & ~0x7fu) != 0) { ++count; bits >>= 7; }
    return count;
}

template<class T, class Predicate = std::function<bool(const T&)>>
class SingleValuePalette final : public Palette<T, Predicate> {
public:
    explicit SingleValuePalette(std::vector<T> entries = {}) {
        if (entries.size() > 1) throw std::invalid_argument("Can't initialize SingleValuePalette with more than one value.");
        if (!entries.empty()) value_ = std::move(entries[0]);
    }
    int idFor(const T& value, PaletteResize<T>& resizeHandler) override {
        if (value_ && !paletteSameObject(*value_, value)) return resizeHandler.onResize(1, value);
        value_ = value; return 0;
    }
    bool maybeHas(Predicate predicate) const override { return predicate(requireValue()); }
    T valueFor(int index) const override {
        if (value_ && index == 0) return *value_;
        throw std::logic_error("Missing Palette entry for id " + std::to_string(index) + ".");
    }
    void read(ByteReader& buffer, const IdMap<T>& map) override { value_ = map.byIdOrThrow(buffer.readVarInt()); }
    void write(ByteWriter& buffer, const IdMap<T>& map) const override { buffer.writeVarInt(map.getId(requireValue())); }
    int getSerializedSize(const IdMap<T>& map) const override { return varIntSize(map.getId(requireValue())); }
    int getSize() const override { return 1; }
    std::unique_ptr<Palette<T, Predicate>> copy() const override {
        return std::make_unique<SingleValuePalette>(*this); // Java returns this; value copy adapts C++ ownership.
    }
private:
    const T& requireValue() const { if (!value_) throw std::logic_error("Use of an uninitialized palette"); return *value_; }
    std::optional<T> value_;
};

template<class T, class Predicate = std::function<bool(const T&)>>
class LinearPalette final : public Palette<T, Predicate> {
public:
    LinearPalette(int bits, std::vector<T> entries = {}) : bits_(bits), values_(capacity(bits)) {
        if (entries.size() > values_.size()) throw std::invalid_argument("LinearPalette entries exceed capacity");
        std::copy(entries.begin(), entries.end(), values_.begin()); size_ = static_cast<int>(entries.size());
    }
    int idFor(const T& value, PaletteResize<T>& resizeHandler) override {
        for (int i = 0; i < size_; ++i) if (paletteSameObject(values_[i], value)) return i;
        if (size_ < static_cast<int>(values_.size())) { values_[size_] = value; return size_++; }
        return resizeHandler.onResize(bits_ + 1, value);
    }
    bool maybeHas(Predicate predicate) const override { for (int i=0;i<size_;++i) if(predicate(values_[i])) return true; return false; }
    T valueFor(int index) const override { if(index>=0 && index<size_) return values_[index]; throw MissingPaletteEntryException(index); }
    void read(ByteReader& buffer, const IdMap<T>& map) override {
        int n=buffer.readVarInt(); if(n<0 || n>static_cast<int>(values_.size())) throw BinaryIOError("invalid LinearPalette size");
        size_=n; for(int i=0;i<size_;++i) values_[i]=map.byIdOrThrow(buffer.readVarInt());
    }
    void write(ByteWriter& buffer, const IdMap<T>& map) const override { buffer.writeVarInt(size_); for(int i=0;i<size_;++i) buffer.writeVarInt(map.getId(values_[i])); }
    int getSerializedSize(const IdMap<T>& map) const override { int n=varIntSize(size_); for(int i=0;i<size_;++i)n+=varIntSize(map.getId(values_[i])); return n; }
    int getSize() const override { return size_; }
    std::unique_ptr<Palette<T, Predicate>> copy() const override { return std::make_unique<LinearPalette>(*this); }
private:
    static std::size_t capacity(int bits) { if(bits<0 || bits>30) throw std::invalid_argument("invalid palette bits"); return std::size_t{1} << bits; }
    int bits_, size_{};
    std::vector<T> values_;
};

template<class T, class Predicate = std::function<bool(const T&)>>
class HashMapPalette final : public Palette<T, Predicate> {
public:
    explicit HashMapPalette(int bits, std::vector<T> entries = {}) : bits_(bits) {
        if(bits<0 || bits>30) throw std::invalid_argument("invalid palette bits");
        for(const auto& entry:entries) add(entry);
    }
    int idFor(const T& value, PaletteResize<T>& resizeHandler) override {
        int id=find(value); if(id<0) { id=add(value); if(id >= (1 << bits_)) id=resizeHandler.onResize(bits_+1,value); } return id;
    }
    bool maybeHas(Predicate predicate) const override { for(const auto& v:values_) if(predicate(v)) return true; return false; }
    T valueFor(int index) const override { if(index<0 || index>=static_cast<int>(values_.size())) throw MissingPaletteEntryException(index); return values_[index]; }
    void read(ByteReader& buffer, const IdMap<T>& map) override {
        values_.clear(); int n=buffer.readVarInt(); if(n<0) throw BinaryIOError("negative palette size");
        for(int i=0;i<n;++i) add(map.byIdOrThrow(buffer.readVarInt()));
    }
    void write(ByteWriter& buffer, const IdMap<T>& map) const override { buffer.writeVarInt(static_cast<int>(values_.size())); for(const auto& v:values_) buffer.writeVarInt(map.getId(v)); }
    int getSerializedSize(const IdMap<T>& map) const override { int n=varIntSize(static_cast<int>(values_.size())); for(const auto& v:values_)n+=varIntSize(map.getId(v)); return n; }
    int getSize() const override { return static_cast<int>(values_.size()); }
    std::unique_ptr<Palette<T, Predicate>> copy() const override { return std::make_unique<HashMapPalette>(*this); }
    const std::vector<T>& getEntries() const noexcept { return values_; }
private:
    int find(const T& value) const { for(int i=0;i<static_cast<int>(values_.size());++i) if(paletteSameObject(values_[i],value)) return i; return -1; }
    int add(const T& value) { const int id=static_cast<int>(values_.size()); values_.push_back(value); return id; }
    int bits_;
    std::vector<T> values_;
};

template<class T, class Predicate = std::function<bool(const T&)>>
class GlobalPalette final : public Palette<T, Predicate> {
public:
    explicit GlobalPalette(const IdMap<T>& registry) : registry_(registry) {}
    int idFor(const T& value, PaletteResize<T>&) override { int id=registry_.getId(value); return id==-1?0:id; }
    bool maybeHas(Predicate) const override { return true; }
    T valueFor(int index) const override { T value=registry_.byId(index); if(!value) throw MissingPaletteEntryException(index); return value; }
    void read(ByteReader&, const IdMap<T>&) override {} // Java GlobalPalette serialization is empty.
    void write(ByteWriter&, const IdMap<T>&) const override {}
    int getSerializedSize(const IdMap<T>&) const override { return 0; }
    int getSize() const override { return registry_.size(); }
    std::unique_ptr<Palette<T, Predicate>> copy() const override { return std::make_unique<GlobalPalette>(*this); }
private:
    const IdMap<T>& registry_;
};

} // namespace carbon
