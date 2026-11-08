#include "carbon/AABB.hpp"
#include "carbon/BinaryIO.hpp"
#include "carbon/BitStorage.hpp"
#include "carbon/Coordinates.hpp"
#include "carbon/CreativeHotbar.hpp"
#include "carbon/Direction.hpp"
#include "carbon/DirectoryResources.hpp"
#include "carbon/Identifier.hpp"
#include "carbon/LegacyRandomSource.hpp"
#include "carbon/Math.hpp"
#include "carbon/Nbt.hpp"
#include "carbon/NbtIO.hpp"
#include "carbon/Noise.hpp"
#include "carbon/Palette.hpp"
#include "carbon/RandomSources.hpp"
#include "carbon/Rotation.hpp"
#include "carbon/ScheduledTicks.hpp"
#include "carbon/Vec3.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    assert(argc == 2);
    // Check that the independently translated libraries compose: packed values
    // survive NBT serialization and can be used with coordinate operations.
    carbon::SingleThreadedRandomSource random(-123456789);
    carbon::SimpleBitStorage storage(5, 4096);
    std::vector<std::int32_t> expected(4096);
    for (int i = 0; i < 4096; ++i) {
        expected[i] = random.nextInt(32);
        storage.set(i, expected[i]);
    }
    std::vector<std::int32_t> unpacked(4096);
    storage.unpack(unpacked);
    assert(unpacked == expected);
    carbon::nbt::CompoundTag root;
    root.put(u"samples", carbon::nbt::Tag(carbon::nbt::IntArray(unpacked)));
    root.putLong(u"position", carbon::BlockPos::asLong(-300, -64, 700));
    auto bytes = carbon::nbt::write(root);
    auto decoded = carbon::nbt::read(bytes);
    assert(decoded == root);
    auto position = carbon::BlockPos::of(*decoded.getLong(u"position"));
    assert(position.x == -300 && position.y == -64 && position.z == 700);
    const carbon::AABB box(-301, -65, 699, -299, -63, 701);
    assert(box.contains(carbon::Vec3(position)));

    // The new standalone primitives also compose with the existing RNG/math
    // and coordinate layer without game or JOML dependencies.
    assert(carbon::rotate(carbon::Rotation::Clockwise90, carbon::Direction::North) == carbon::Direction::East);
    assert(carbon::mirror(carbon::Mirror::FrontBack, carbon::Direction::East) == carbon::Direction::West);
    assert(carbon::directionById(-1) == carbon::Direction::East);
    carbon::CreativeHotbar hotbar;
    hotbar.selectSlot(8);
    hotbar.scroll(1.0);
    assert(hotbar.selectedSlot == 7);
    assert(hotbar.pickBlock(carbon::BlockId::StoneSlab));
    assert(hotbar.selectedSlot == 5); // existing supported item is selected
    carbon::LegacyRandomSource noiseRandom(42);
    carbon::SimplexNoise noise(noiseRandom);
    assert(std::isfinite(noise.get(0.25, -0.75, 1.5)));
    carbon::SingleValuePalette<int*> palette({&expected[0]});
    assert(palette.valueFor(0) == &expected[0] && palette.getSize() == 1);

    carbon::LevelChunkTicks ticks({
        {1, {-300, -64, 700}, 2, carbon::TickPriority::High}
    });
    assert(ticks.count() == 1 && ticks.hasScheduledTick({-300, -64, 700}, 1));
    ticks.unpack(40);
    const auto scheduled = ticks.poll();
    assert(scheduled && scheduled->triggerTick == 42 && scheduled->priority == carbon::TickPriority::High);

    carbon::DirectoryResources resources(argv[1]);
    const auto id = carbon::Identifier::tryParse("minecraft:waypoint_style/default.json");
    assert(id);
    const auto original = resources.read(carbon::ResourceTree::assets, *id);
    assert(original && !original->empty());
    std::cout << "PASS: core headers, packed values, NBT IO, coordinates, direction/rotation, noise, palettes, geometry and original resource access compose\n";
}
