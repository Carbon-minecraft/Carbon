#include "carbon/ChunkSection.hpp"
#include "carbon/Heightmap.hpp"

#include <cassert>
#include <memory>
#include <vector>

namespace {
struct Fluid { bool empty; bool ticking; };
struct State { int id; bool air; bool ticking; Fluid fluid; };
struct Biome { int id; };

template<class T> struct Map final : carbon::IdMap<std::shared_ptr<T>> {
    std::vector<std::shared_ptr<T>> values;
    int getId(const std::shared_ptr<T>& value) const override {
        for (int i = 0; i < static_cast<int>(values.size()); ++i) if (values[i].get() == value.get()) return i;
        return -1;
    }
    std::shared_ptr<T> byId(int id) const override { return id >= 0 && id < static_cast<int>(values.size()) ? values[id] : nullptr; }
    int size() const override { return static_cast<int>(values.size()); }
};
}

int main() {
    // These are native assertions transcribed from the supplied Java methods;
    // no JVM oracle is claimed for this batch.
    using Handle = std::shared_ptr<State>;
    using BiomeHandle = std::shared_ptr<Biome>;
    Map<State> states;
    auto air = std::make_shared<State>(State{0, true, false, {true, false}});
    auto stone = std::make_shared<State>(State{1, false, false, {true, false}});
    auto tickFluid = std::make_shared<State>(State{2, false, true, {false, true}});
    states.values = {air, stone, tickFluid};
    Map<Biome> biomes;
    auto plains = std::make_shared<Biome>(Biome{0});
    auto forest = std::make_shared<Biome>(Biome{1});
    biomes.values = {plains, forest};

    auto stateStrategy = carbon::PalettedContainerStrategy<Handle>::forBlockStates(states);
    auto biomeStrategy = carbon::PalettedContainerStrategy<BiomeHandle>::forBiomes(biomes);
    carbon::PalettedContainer<Handle> stateData(air, stateStrategy);
    carbon::PalettedContainer<BiomeHandle> biomeData(plains, biomeStrategy);
    carbon::ChunkSectionStateTraits<Handle, Fluid> traits{
        [](const Handle& s) { return s->air; },
        [](const Handle& s) { return s->ticking; },
        [](const Handle& s) { return s->fluid; },
        [](const Fluid& f) { return f.empty; },
        [](const Fluid& f) { return f.ticking; }
    };
    carbon::LevelChunkSection<Handle, BiomeHandle, Fluid> section(std::move(stateData), std::move(biomeData), traits);
    assert(section.hasOnlyAir() && !section.hasFluid() && !section.isRandomlyTicking());
    section.setBlockState(15, 15, 15, stone);
    section.setBlockState(0, 0, 0, tickFluid);
    assert(section.nonEmptyBlockCount() == 2 && section.fluidCount() == 1);
    assert(section.tickingBlockCount() == 1 && section.tickingFluidCount() == 1);
    section.setBlockState(0, 0, 0, air);
    assert(section.nonEmptyBlockCount() == 1 && !section.hasFluid() && !section.isRandomlyTicking());
    section.setBlockState(15, 15, 15, air);
    assert(section.hasOnlyAir());
    auto sectionCopy = section.copy();
    sectionCopy.setBlockState(1, 2, 3, stone);
    assert(section.hasOnlyAir() && sectionCopy.nonEmptyBlockCount() == 1);
    section.fillBiomesFromNoise([&](int x, int y, int z) { return ((x + y + z) & 1) ? forest : plains; }, 4, -3, 8);
    assert(section.getNoiseBiome(0, 0, 0).get() == forest.get());

    carbon::ByteWriter writer;
    section.getStates().write(writer);
    section.getBiomes().write(writer);
    carbon::ByteReader reader(writer.bytes());
    carbon::PalettedContainer<Handle> restoredStates(air, stateStrategy);
    carbon::PalettedContainer<BiomeHandle> restoredBiomes(plains, biomeStrategy);
    restoredStates.read(reader);
    restoredBiomes.read(reader);
    assert(restoredStates.get(15, 15, 15).get() == air.get());
    assert(restoredBiomes.get(0, 0, 0).get() == forest.get());

    carbon::ByteWriter sectionWriter;
    section.write(sectionWriter);
    assert(section.serializedSize() == sectionWriter.bytes().size());
    carbon::LevelChunkSection<Handle, BiomeHandle, Fluid> sectionRead(
        carbon::PalettedContainer<Handle>(air, stateStrategy),
        carbon::PalettedContainer<BiomeHandle>(plains, biomeStrategy), traits);
    carbon::ByteReader sectionReader(sectionWriter.bytes());
    sectionRead.read(sectionReader);
    assert(sectionReader.remaining() == 0 && sectionRead.hasOnlyAir() && !sectionRead.hasFluid());
    assert(sectionRead.getNoiseBiome(0, 0, 0).get() == forest.get());

    carbon::LevelHeightAccessor level(-64, 384);
    std::vector<int> world(16 * 16 * 384, 0);
    const auto at = [](int x, int y, int z) { return (y + 64) * 256 + z * 16 + x; };
    auto get = [&](int x, int y, int z) { return world.at(at(x, y, z)); };
    carbon::Heightmap<int> heightmap(level, [](int id) { return id == 1; }, get);
    auto put = [&](int x, int y, int z, int id) { world.at(at(x, y, z)) = id; };
    put(2, -64, 3, 1);
    put(2, 319, 3, 1);
    heightmap.prime(304, [](int id) { return id == 0; });
    assert(heightmap.getFirstAvailable(2, 3) == 320);
    put(2, 319, 3, 0);
    assert(heightmap.update(2, 319, 3, 0));
    assert(heightmap.getFirstAvailable(2, 3) == -63);
    assert(heightmap.getHighestTaken(2, 3) == -64);
    put(2, 5, 3, 1);
    assert(heightmap.update(2, 5, 3, 1));
    assert(heightmap.getFirstAvailable(2, 3) == 6);
    const auto words = heightmap.getRawData();
    carbon::Heightmap<int> copy(level, [](int id) { return id == 1; }, get);
    assert(copy.setRawData(words));
    assert(copy.getFirstAvailable(2, 3) == 6);
    assert(!copy.setRawData({1, 2, 3}));
}
