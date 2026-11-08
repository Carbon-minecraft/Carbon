#pragma once

// Source-derived chunk-section subset from Java 26.3 LevelChunkSection.
// Registry bootstrapping, codecs, networking and paletted-container locking are
// intentionally outside this layer. State and biome values are caller-owned
// identity handles compatible with carbon::IdMap.

#include "carbon/PalettedContainer.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>

namespace carbon {

template<class State, class Fluid>
struct ChunkSectionStateTraits {
    std::function<bool(const State&)> isAir;
    std::function<bool(const State&)> isRandomlyTicking;
    std::function<Fluid(const State&)> fluidState;
    std::function<bool(const Fluid&)> fluidIsEmpty;
    std::function<bool(const Fluid&)> fluidIsRandomlyTicking;
};

template<class State, class Biome, class Fluid,
         class StatePredicate = std::function<bool(const State&)>,
         class BiomePredicate = std::function<bool(const Biome&)>>
class LevelChunkSection {
public:
    static constexpr int BIOME_CONTAINER_BITS = 2;
    static constexpr int STATE_COUNT = 4096;
    static constexpr int BIOME_COUNT = 64;
    using States = PalettedContainer<State, StatePredicate>;
    using Biomes = PalettedContainer<Biome, BiomePredicate>;
    using BiomeResolver = std::function<Biome(int quartX, int quartY, int quartZ)>;

    LevelChunkSection(States states, Biomes biomes, ChunkSectionStateTraits<State, Fluid> traits)
        : states_(std::move(states)), biomes_(std::make_unique<Biomes>(std::move(biomes))), traits_(std::move(traits)) {
        requireTraits();
        recalcBlockCounts();
    }

    State getBlockState(int sectionX, int sectionY, int sectionZ) const {
        return states_.get(sectionX, sectionY, sectionZ);
    }
    Fluid getFluidState(int sectionX, int sectionY, int sectionZ) const {
        return traits_.fluidState(getBlockState(sectionX, sectionY, sectionZ));
    }
    State setBlockState(int sectionX, int sectionY, int sectionZ, const State& state) {
        State previous = states_.getAndSet(sectionX, sectionY, sectionZ, state);
        removeCounts(previous);
        addCounts(state);
        return previous;
    }
    bool hasOnlyAir() const noexcept { return nonEmptyBlockCount_ == 0; }
    bool hasFluid() const noexcept { return fluidCount_ > 0; }
    bool isRandomlyTicking() const noexcept { return isRandomlyTickingBlocks() || isRandomlyTickingFluids(); }
    bool isRandomlyTickingBlocks() const noexcept { return tickingBlockCount_ > 0; }
    bool isRandomlyTickingFluids() const noexcept { return tickingFluidCount_ > 0; }

    void recalcBlockCounts() {
        nonEmptyBlockCount_ = fluidCount_ = tickingBlockCount_ = tickingFluidCount_ = 0;
        for (int i = 0; i < states_.entryCount(); ++i) addCounts(states_.getByIndex(i));
    }
    States& getStates() noexcept { return states_; }
    const States& getStates() const noexcept { return states_; }
    Biomes& getBiomes() noexcept { return *biomes_; }
    const Biomes& getBiomes() const noexcept { return *biomes_; }
    Biome getNoiseBiome(int quartX, int quartY, int quartZ) const { return biomes_->get(quartX, quartY, quartZ); }

    void fillBiomesFromNoise(const BiomeResolver& resolver, int quartMinX, int quartMinY, int quartMinZ) {
        auto next = std::make_unique<Biomes>(biomes_->recreate());
        for (int x = 0; x < 4; ++x)
            for (int y = 0; y < 4; ++y)
                for (int z = 0; z < 4; ++z)
                    next->getAndSetUnchecked(x, y, z, resolver(quartMinX + x, quartMinY + y, quartMinZ + z));
        biomes_ = std::move(next);
    }
    LevelChunkSection copy() const { return LevelChunkSection(states_.copy(), biomes_->copy(), traits_); }

    // FriendlyByteBuf layout from Java: two signed shorts followed by the two
    // PalettedContainer encodings. Ticking counters are derived and not stored.
    void write(ByteWriter& buffer) const {
        buffer.writeShort(nonEmptyBlockCount_);
        buffer.writeShort(fluidCount_);
        states_.write(buffer);
        biomes_->write(buffer);
    }
    std::size_t serializedSize() const noexcept {
        return 4u + states_.serializedSize() + biomes_->serializedSize();
    }
    void read(ByteReader& buffer) {
        nonEmptyBlockCount_ = buffer.readShort();
        fluidCount_ = buffer.readShort();
        states_.read(buffer);
        auto next = std::make_unique<Biomes>(biomes_->recreate());
        next->read(buffer);
        biomes_ = std::move(next);
    }
    void readBiomes(ByteReader& buffer) {
        auto next = std::make_unique<Biomes>(biomes_->recreate());
        next->read(buffer);
        biomes_ = std::move(next);
    }

    std::int16_t nonEmptyBlockCount() const noexcept { return nonEmptyBlockCount_; }
    std::int16_t fluidCount() const noexcept { return fluidCount_; }
    std::int16_t tickingBlockCount() const noexcept { return tickingBlockCount_; }
    std::int16_t tickingFluidCount() const noexcept { return tickingFluidCount_; }

private:
    void requireTraits() const {
        if (!traits_.isAir || !traits_.isRandomlyTicking || !traits_.fluidState ||
            !traits_.fluidIsEmpty || !traits_.fluidIsRandomlyTicking)
            throw std::invalid_argument("LevelChunkSection requires complete state and fluid traits");
    }
    void addCounts(const State& state) {
        if (traits_.isAir(state)) return;
        ++nonEmptyBlockCount_;
        if (traits_.isRandomlyTicking(state)) ++tickingBlockCount_;
        const Fluid fluid = traits_.fluidState(state);
        if (!traits_.fluidIsEmpty(fluid)) {
            ++fluidCount_;
            if (traits_.fluidIsRandomlyTicking(fluid)) ++tickingFluidCount_;
        }
    }
    void removeCounts(const State& state) {
        if (traits_.isAir(state)) return;
        --nonEmptyBlockCount_;
        if (traits_.isRandomlyTicking(state)) --tickingBlockCount_;
        const Fluid fluid = traits_.fluidState(state);
        if (!traits_.fluidIsEmpty(fluid)) {
            --fluidCount_;
            if (traits_.fluidIsRandomlyTicking(fluid)) --tickingFluidCount_;
        }
    }

    States states_;
    std::unique_ptr<Biomes> biomes_;
    ChunkSectionStateTraits<State, Fluid> traits_;
    std::int16_t nonEmptyBlockCount_{};
    std::int16_t fluidCount_{};
    std::int16_t tickingBlockCount_{};
    std::int16_t tickingFluidCount_{};
};

} // namespace carbon
