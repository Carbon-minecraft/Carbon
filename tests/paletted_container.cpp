#include "carbon/PalettedContainer.hpp"
#include <cassert>
#include <memory>
#include <string>
#include <vector>

using Value = std::shared_ptr<std::string>;
class Registry final : public carbon::IdMap<Value> {
public:
    std::vector<Value> entries;
    int getId(const Value& v) const override { for (int i=0;i<(int)entries.size();++i) if(entries[i].get()==v.get()) return i; return -1; }
    Value byId(int i) const override { return i>=0 && i<(int)entries.size()?entries[i]:Value{}; }
    int size() const override { return static_cast<int>(entries.size()); }
};

int main() {
    Registry registry;
    for (int i = 0; i < 600; ++i) registry.entries.push_back(std::make_shared<std::string>(std::to_string(i)));
    using BlockStrategy = carbon::PalettedContainerStrategy<Value>;
    auto blocks = BlockStrategy::forBlockStates(registry);
    auto biomes = BlockStrategy::forBiomes(registry);
    assert(blocks.entryCount() == 4096 && blocks.bitsPerAxis() == 4);
    assert(biomes.entryCount() == 64 && biomes.bitsPerAxis() == 2);
    assert(blocks.index(3, 2, 1) == 2 * 256 + 1 * 16 + 3);
    assert(biomes.index(3, 2, 1) == 39);
    bool badCoord=false; try { (void)blocks.index(16,0,0); } catch(const std::out_of_range&) { badCoord=true; } assert(badCoord);

    // Configuration thresholds lifted from supplied chunk/Strategy.java.
    assert(blocks.configurationForBitCount(0).bitsInMemory == 0);
    assert(blocks.configurationForBitCount(1).bitsInMemory == 4);
    assert(blocks.configurationForBitCount(4).bitsInMemory == 4);
    assert(blocks.configurationForBitCount(5).bitsInMemory == 5);
    assert(blocks.configurationForBitCount(9).global && blocks.configurationForBitCount(9).bitsInMemory == 10);
    assert(biomes.configurationForBitCount(1).bitsInMemory == 1);
    assert(biomes.configurationForBitCount(3).bitsInMemory == 3);
    assert(biomes.configurationForBitCount(4).global);

    carbon::PalettedContainer<Value> c(registry.entries[0], blocks);
    assert(c.bitsPerEntry() == 0 && c.get(15,15,15).get() == registry.entries[0].get());
    auto previous=c.getAndSetUnchecked(3,2,1,registry.entries[1]);
    assert(previous.get() == registry.entries[0].get() && c.bitsPerEntry() == 4);
    c.set(0,0,0,registry.entries[2]);
    assert(c.get(3,2,1).get() == registry.entries[1].get());
    auto copied=c.copy(); copied.set(3,2,1,registry.entries[3]);
    assert(c.get(3,2,1).get() == registry.entries[1].get());
    assert(copied.get(3,2,1).get() == registry.entries[3].get());
    std::vector<Value> all;
    c.getAll([&](const Value& v) { all.push_back(v); });
    assert(all.size() == 3);
    const auto packed=c.pack();
    assert(packed.bitsPerEntry == 4 && packed.paletteEntries.size() == 3 && packed.storage.has_value());
    auto restored=carbon::PalettedContainer<Value>::unpack(blocks,packed);
    assert(restored.get(0,0,0).get() == registry.entries[2].get());
    assert(restored.get(3,2,1).get() == registry.entries[1].get());
    carbon::ByteWriter wire; c.write(wire); assert(wire.bytes().size() == c.serializedSize());
    carbon::PalettedContainer<Value> wireCopy(registry.entries[0], blocks);
    carbon::ByteReader in(wire.bytes()); wireCopy.read(in);
    assert(in.remaining()==0 && wireCopy.get(3,2,1).get()==registry.entries[1].get());
    carbon::PalettedContainer<Value> uniform(registry.entries[5], blocks);
    auto uniformPacked=uniform.pack();
    assert(uniformPacked.bitsPerEntry == 0 && uniformPacked.paletteEntries.size() == 1 && !uniformPacked.storage);
    auto uniformCopy=carbon::PalettedContainer<Value>::unpack(blocks,uniformPacked);
    assert(uniformCopy.bitsPerEntry()==0 && uniformCopy.get(15,15,15).get()==registry.entries[5].get());

    // Fill the block-state 4-bit linear palette, then the 8-bit hashmap. The
    // 257th identity crosses into the global registry palette, preserving IDs.
    carbon::PalettedContainer<Value> many(registry.entries[0], blocks);
    for (int i=1;i<=256;++i) many.set((i-1)%16,0,(i-1)/16,registry.entries[i]);
    assert(many.bitsPerEntry() == 10 && many.paletteSize() == registry.size());
    assert(many.get(15,0,15).get() == registry.entries[256].get());
    const auto globalPacked=many.pack();
    assert(globalPacked.paletteEntries.size() == 257 && globalPacked.bitsPerEntry == 9 && globalPacked.storage);
    auto globalRestored=carbon::PalettedContainer<Value>::unpack(blocks,globalPacked);
    assert(globalRestored.bitsPerEntry() == 10 && globalRestored.get(15,0,15).get() == registry.entries[256].get());

    // Biome strategy uses 4^3 entries and transitions to global at 9 values.
    carbon::PalettedContainer<Value> bio(registry.entries[0], biomes);
    for (int i=1;i<=7;++i) bio.set((i-1)%4,0,(i-1)/4,registry.entries[i]);
    assert(bio.bitsPerEntry() == 3);
    bio.set(0,1,0,registry.entries[8]);
    assert(bio.bitsPerEntry() == 10 && bio.get(0,1,0).get() == registry.entries[8].get());
    auto bioPacked=bio.pack();
    assert(bioPacked.bitsPerEntry == 4 && bioPacked.storage);
    auto bioRestored=carbon::PalettedContainer<Value>::unpack(biomes,bioPacked);
    assert(bioRestored.get(0,1,0).get() == registry.entries[8].get());

    bool badBits=false; auto invalid=packed; invalid.bitsPerEntry=7;
    try { (void)carbon::PalettedContainer<Value>::unpack(blocks,invalid); } catch(const carbon::BinaryIOError&) { badBits=true; } assert(badBits);
}
