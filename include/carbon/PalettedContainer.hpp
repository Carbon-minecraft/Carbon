#pragma once

// Integration of the supplied Java 26.3 chunk Strategy/Configuration and
// PalettedContainer behavior with carbon::Palette and carbon::BitStorage.
#include "carbon/BitStorage.hpp"
#include "carbon/Palette.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace carbon {

enum class PaletteDomain { BlockStates, Biomes };

// Strategy thresholds from net.minecraft.world.level.chunk.Strategy. The IdMap
// is borrowed and must outlive this object and every container using it.
template<class T, class Predicate = std::function<bool(const T&)>>
class PalettedContainerStrategy {
public:
    static PalettedContainerStrategy forBlockStates(const IdMap<T>& map) { return {map, PaletteDomain::BlockStates}; }
    static PalettedContainerStrategy forBiomes(const IdMap<T>& map) { return {map, PaletteDomain::Biomes}; }

    int entryCount() const noexcept { return entryCount_; }
    int bitsPerAxis() const noexcept { return bitsPerAxis_; }
    int index(int x, int y, int z) const {
        const int limit = 1 << bitsPerAxis_;
        if (x < 0 || y < 0 || z < 0 || x >= limit || y >= limit || z >= limit)
            throw std::out_of_range("coordinate outside paletted container bounds");
        return (y << (bitsPerAxis_ * 2)) | (z << bitsPerAxis_) | x;
    }
    const IdMap<T>& globalMap() const noexcept { return map_; }
    int globalPaletteBitsInMemory() const noexcept { return globalBits_; }
    PaletteDomain domain() const noexcept { return domain_; }
    int minimumBitsForPaletteSize(int size) const {
        if (size < 0) throw std::invalid_argument("negative palette size");
        if (size <= 1) return 0;
        return static_cast<int>(std::bit_width(static_cast<unsigned>(size - 1)));
    }

    struct Configuration {
        int bitsInMemory{};
        int bitsInStorage{};
        bool global{};
        bool alwaysRepack{};
    };
    Configuration configurationForBitCount(int bits) const {
        if (bits < 0) throw std::invalid_argument("negative palette bit count");
        if (domain_ == PaletteDomain::BlockStates) {
            if (bits == 0) return {0, 0, false, false};
            if (bits <= 4) return {4, 4, false, false};
            if (bits <= 8) return {bits, bits, false, false};
        } else {
            if (bits <= 0) return {0, 0, false, false};
            if (bits <= 3) return {bits, bits, false, false};
        }
        return {globalBits_, bits, true, true};
    }
    Configuration configurationForPaletteSize(int size) const { return configurationForBitCount(minimumBitsForPaletteSize(size)); }

    std::unique_ptr<Palette<T, Predicate>> createPalette(const Configuration& config, const std::vector<T>& entries = {}) const {
        if (config.global) return std::make_unique<GlobalPalette<T, Predicate>>(map_);
        if (config.bitsInMemory == 0) return std::make_unique<SingleValuePalette<T, Predicate>>(entries);
        if ((domain_ == PaletteDomain::Biomes && config.bitsInMemory <= 3) ||
            (domain_ == PaletteDomain::BlockStates && config.bitsInMemory == 4))
            return std::make_unique<LinearPalette<T, Predicate>>(config.bitsInMemory, entries);
        return std::make_unique<HashMapPalette<T, Predicate>>(config.bitsInMemory, entries);
    }
private:
    PalettedContainerStrategy(const IdMap<T>& map, PaletteDomain domain)
        : map_(map), domain_(domain), bitsPerAxis_(domain == PaletteDomain::BlockStates ? 4 : 2),
          entryCount_(1 << (bitsPerAxis_ * 3)), globalBits_(minimumBitsForMap(map.size())) {}
    static int minimumBitsForMap(int count) {
        if (count < 0) throw std::invalid_argument("negative global map size");
        if (count <= 1) return 0;
        return static_cast<int>(std::bit_width(static_cast<unsigned>(count - 1)));
    }
    const IdMap<T>& map_;
    PaletteDomain domain_;
    int bitsPerAxis_, entryCount_, globalBits_;
};

template<class T>
struct PalettedContainerPackedData {
    std::vector<T> paletteEntries;
    std::optional<std::vector<std::uint64_t>> storage;
    int bitsPerEntry{-1};
};

template<class T, class Predicate = std::function<bool(const T&)>>
class PalettedContainer final : public PaletteResize<T> {
    using Strategy = PalettedContainerStrategy<T, Predicate>;
    using PaletteType = Palette<T, Predicate>;
    struct Data {
        typename Strategy::Configuration configuration;
        std::unique_ptr<BitStorage> storage;
        std::unique_ptr<PaletteType> palette;
    };
public:
    PalettedContainer(const T& initialValue, const Strategy& strategy) : strategy_(strategy) {
        data_ = createData(nullptr, 0);
        data_->palette->idFor(initialValue, *this);
    }
    PalettedContainer(const PalettedContainer& other) : strategy_(other.strategy_), data_(copyData(*other.data_)) {}
    PalettedContainer(PalettedContainer&&) noexcept = default;
    PalettedContainer& operator=(const PalettedContainer&) = delete;

    int onResize(int bits, const T& lastAddedValue) override {
        auto next = createData(data_.get(), bits);
        copyContents(*data_, *next);
        data_ = std::move(next);
        return data_->palette->idFor(lastAddedValue, noResizeExpected());
    }

    T get(int x, int y, int z) const { return get(strategy_.index(x, y, z)); }
    T getUnchecked(int x, int y, int z) const { return get(strategy_.index(x, y, z)); }
    void set(int x, int y, int z, const T& value) { const int i = strategy_.index(x, y, z); setIndex(i, value); }
    T getAndSet(int x, int y, int z, const T& value) {
        const int i = strategy_.index(x, y, z);
        const int id = data_->palette->idFor(value, *this);
        return data_->palette->valueFor(data_->storage->getAndSet(i, id));
    }
    T getAndSetUnchecked(int x, int y, int z, const T& value) {
        return getAndSet(x, y, z, value);
    }
    void setUnchecked(int x, int y, int z, const T& value) { setIndex(strategy_.index(x, y, z), value); }
    T getByIndex(int index) const { checkIndex(index); return data_->palette->valueFor(data_->storage->get(index)); }

    int bitsPerEntry() const noexcept { return data_->storage->getBits(); }
    int paletteSize() const noexcept { return data_->palette->getSize(); }
    int entryCount() const noexcept { return strategy_.entryCount(); }
    bool maybeHas(Predicate predicate) const { return data_->palette->maybeHas(std::move(predicate)); }
    void forEachInPalette(const std::function<void(const T&)>& consumer) const {
        for (int i = 0; i < data_->palette->getSize(); ++i) consumer(data_->palette->valueFor(i));
    }
    void getAll(const std::function<void(const T&)>& consumer) const {
        std::vector<bool> visited(static_cast<std::size_t>(data_->palette->getSize()));
        data_->storage->getAll([&](int id) {
            if (id < 0 || id >= static_cast<int>(visited.size())) { (void)data_->palette->valueFor(id); }
            if (!visited[static_cast<std::size_t>(id)]) {
                visited[static_cast<std::size_t>(id)] = true;
                consumer(data_->palette->valueFor(id));
            }
        });
    }
    PalettedContainer copy() const { return PalettedContainer(*this); }
    PalettedContainer recreate() const { return PalettedContainer(data_->palette->valueFor(0), strategy_); }

    void write(ByteWriter& buffer) const {
        buffer.writeByte(static_cast<std::int8_t>(data_->storage->getBits()));
        data_->palette->write(buffer, strategy_.globalMap());
        for (const auto value : data_->storage->getRaw()) buffer.writeLong(std::bit_cast<std::int64_t>(value));
    }
    std::size_t serializedSize() const noexcept {
        return 1u + static_cast<std::size_t>(data_->palette->getSerializedSize(strategy_.globalMap())) + data_->storage->getRaw().size() * 8u;
    }
    void read(ByteReader& buffer) {
        const int bits = buffer.readByte();
        auto next = createData(data_.get(), bits);
        next->palette->read(buffer, strategy_.globalMap());
        for (auto& value : next->storage->getRaw()) value = std::bit_cast<std::uint64_t>(buffer.readLong());
        data_ = std::move(next);
    }

    PalettedContainerPackedData<T> pack() const {
        HashMapPalette<T, Predicate> packedPalette(data_->storage->getBits());
        const auto ids = reencodeContents(*data_->storage, *data_->palette, packedPalette);
        const auto config = strategy_.configurationForPaletteSize(packedPalette.getSize());
        PalettedContainerPackedData<T> result;
        result.paletteEntries = packedPalette.getEntries();
        result.bitsPerEntry = config.bitsInStorage;
        if (result.bitsPerEntry != 0) {
            SimpleBitStorage storage(result.bitsPerEntry, strategy_.entryCount(), ids);
            result.storage = storage.getRaw();
        }
        return result;
    }
    static PalettedContainer unpack(const Strategy& strategy, const PalettedContainerPackedData<T>& packed) {
        const auto config = strategy.configurationForPaletteSize(static_cast<int>(packed.paletteEntries.size()));
        const int bitsOnDisc = config.bitsInStorage;
        if (packed.bitsPerEntry != -1 && packed.bitsPerEntry != bitsOnDisc)
            throw BinaryIOError("Invalid bit count, calculated " + std::to_string(bitsOnDisc) + ", but container declared " + std::to_string(packed.bitsPerEntry));
        if (config.bitsInMemory == 0) {
            if (packed.paletteEntries.size() != 1) throw BinaryIOError("single value palette must contain exactly one value");
            return PalettedContainer(strategy, config, std::make_unique<ZeroBitStorage>(strategy.entryCount()), strategy.createPalette(config, packed.paletteEntries));
        }
        if (!packed.storage) throw BinaryIOError("Missing values for non-zero storage");
        std::unique_ptr<PaletteType> palette;
        std::unique_ptr<BitStorage> storage;
        try {
            if (!config.alwaysRepack && config.bitsInMemory == bitsOnDisc) {
                palette = strategy.createPalette(config, packed.paletteEntries);
                storage = std::make_unique<SimpleBitStorage>(config.bitsInMemory, strategy.entryCount(), *packed.storage);
            } else {
                HashMapPalette<T, Predicate> oldPalette(bitsOnDisc, packed.paletteEntries);
                SimpleBitStorage oldStorage(bitsOnDisc, strategy.entryCount(), *packed.storage);
                palette = strategy.createPalette(config, packed.paletteEntries);
                const auto contents = reencodeContents(oldStorage, oldPalette, *palette);
                storage = std::make_unique<SimpleBitStorage>(config.bitsInMemory, strategy.entryCount(), contents);
            }
        } catch (const std::exception& e) { throw BinaryIOError(std::string("Failed to read PalettedContainer: ") + e.what()); }
        return PalettedContainer(strategy, config, std::move(storage), std::move(palette));
    }
private:
    PalettedContainer(const Strategy& strategy, typename Strategy::Configuration config,
                      std::unique_ptr<BitStorage> storage, std::unique_ptr<PaletteType> palette)
        : strategy_(strategy), data_(std::make_unique<Data>(Data{config, std::move(storage), std::move(palette)})) {}
    static PaletteResize<T>& noResizeExpected() {
        class NoResize final : public PaletteResize<T> { public: int onResize(int, const T&) override { throw std::logic_error("unexpected palette resize"); } };
        static NoResize handler; return handler;
    }
    static std::unique_ptr<Data> copyData(const Data& d) {
        return std::make_unique<Data>(Data{d.configuration, d.storage->copy(), d.palette->copy()});
    }
    std::unique_ptr<Data> createData(const Data* old, int bits) const {
        const auto config = strategy_.configurationForBitCount(bits);
        if (old && old->configuration.bitsInMemory == config.bitsInMemory && old->configuration.bitsInStorage == config.bitsInStorage && old->configuration.global == config.global)
            return copyData(*old);
        std::unique_ptr<BitStorage> storage;
        if (config.bitsInMemory == 0) storage = std::make_unique<ZeroBitStorage>(strategy_.entryCount());
        else storage = std::make_unique<SimpleBitStorage>(config.bitsInMemory, strategy_.entryCount());
        auto palette = strategy_.createPalette(config);
        return std::make_unique<Data>(Data{config, std::move(storage), std::move(palette)});
    }
    static void copyContents(const Data& old, Data& next) {
        auto& storage = *next.storage;
        if (old.palette->getSize() == 1) {
            storage.fill(next.palette->idFor(old.palette->valueFor(0), noResizeExpected()));
        } else {
            int last = -1, newId = -1;
            for (int i = 0; i < old.storage->getSize(); ++i) {
                const int oldId = old.storage->get(i);
                if (oldId != last) { newId = next.palette->idFor(old.palette->valueFor(oldId), noResizeExpected()); last = oldId; }
                storage.set(i, newId);
            }
        }
    }
    static std::vector<int> reencodeContents(const BitStorage& storage, const PaletteType& oldPalette, PaletteType& newPalette) {
        std::vector<int> result(static_cast<std::size_t>(storage.getSize()));
        PaletteResize<T>& noResize = noResizeExpected();
        int lastRead = -1, lastWritten = -1;
        for (int i = 0; i < storage.getSize(); ++i) {
            const int id = storage.get(i);
            if (id != lastRead) { lastRead = id; lastWritten = newPalette.idFor(oldPalette.valueFor(id), noResize); }
            result[static_cast<std::size_t>(i)] = lastWritten;
        }
        return result;
    }
    void checkIndex(int index) const { if (index < 0 || index >= strategy_.entryCount()) throw std::out_of_range("index outside storage bounds"); }
    void setIndex(int index, const T& value) { checkIndex(index); const int id = data_->palette->idFor(value, *this); data_->storage->set(index, id); }
    T get(int index) const { checkIndex(index); return data_->palette->valueFor(data_->storage->get(index)); }

    const Strategy& strategy_;
    std::unique_ptr<Data> data_;
};

} // namespace carbon

