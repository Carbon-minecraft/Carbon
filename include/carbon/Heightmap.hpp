#pragma once

// Source-derived subset of net.minecraft.world.level.levelgen.Heightmap.
// Heightmap type/tag predicates and chunk lifecycle are supplied by the caller.

#include "carbon/BitStorage.hpp"
#include "carbon/LevelHeightAccessor.hpp"

#include <bit>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace carbon {

template<class State>
class Heightmap {
public:
    using OpaquePredicate = std::function<bool(const State&)>;
    // Java's primeHeightmaps constructs local BlockPos(x,y,z), then queries the
    // chunk. The callback receives those same local x/z and world y coordinates.
    using BlockGetter = std::function<State(int localX, int worldY, int localZ)>;
    using AirPredicate = std::function<bool(const State&)>;

    Heightmap(LevelHeightAccessor height, OpaquePredicate isOpaque, BlockGetter blockGetter)
        : height_(height), isOpaque_(std::move(isOpaque)), blockGetter_(std::move(blockGetter)),
          data_(heightBits(height.getHeight()), 256) {
        if (!isOpaque_ || !blockGetter_) throw std::invalid_argument("Heightmap requires predicates and block getter");
    }

    bool update(int localX, int worldY, int localZ, const State& state) {
        const int firstAvailable = getFirstAvailable(localX, localZ);
        if (worldY <= firstAvailable - 2) return false;
        if (isOpaque_(state)) {
            if (worldY >= firstAvailable) {
                setHeight(localX, localZ, worldY + 1);
                return true;
            }
        } else if (firstAvailable - 1 == worldY) {
            for (int y = worldY - 1; y >= height_.getMinY(); --y) {
                const State below = blockGetter_(localX, y, localZ);
                if (isOpaque_(below)) {
                    setHeight(localX, localZ, y + 1);
                    return true;
                }
            }
            setHeight(localX, localZ, height_.getMinY());
            return true;
        }
        return false;
    }

    int getFirstAvailable(int x, int z) const { return height_.getMinY() + data_.get(index(x, z)); }
    int getHighestTaken(int x, int z) const { return getFirstAvailable(x, z) - 1; }
    const std::vector<std::uint64_t>& getRawData() const noexcept { return data_.getRaw(); }
    std::vector<std::uint64_t>& getRawData() noexcept { return data_.getRaw(); }

    // Source behavior copies exact-sized raw words; on a mismatch Java warns
    // and primes from the chunk. The callback keeps that lifecycle decision
    // explicit and allows the caller to invoke prime() for this map.
    bool setRawData(const std::vector<std::uint64_t>& raw, const std::function<void()>& onSizeMismatch = {}) {
        auto& target = data_.getRaw();
        if (target.size() != raw.size()) {
            if (onSizeMismatch) onSizeMismatch();
            return false;
        }
        target = raw;
        return true;
    }

    // highestSectionPosition is the base Y of the highest section, matching
    // Java's +16 upper bound. Empty/uninitialized columns remain at minY.
    void prime(int highestSectionPosition, const AirPredicate& isAir) {
        if (!isAir) throw std::invalid_argument("Heightmap prime requires air predicate");
        const int highestExclusive = highestSectionPosition + 16;
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                for (int y = highestExclusive - 1; y >= height_.getMinY(); --y) {
                    const State state = blockGetter_(x, y, z);
                    if (!isAir(state) && isOpaque_(state)) {
                        setHeight(x, z, y + 1);
                        break;
                    }
                }
            }
        }
    }

private:
    static int heightBits(int height) {
        if (height <= 0) throw std::invalid_argument("Heightmap height must be positive");
        return static_cast<int>(std::bit_width(static_cast<unsigned>(height)));
    }
    static int index(int x, int z) {
        if (x < 0 || x >= 16 || z < 0 || z >= 16) throw std::out_of_range("heightmap column outside 16x16 bounds");
        return x + z * 16;
    }
    void setHeight(int x, int z, int worldHeight) { data_.set(index(x, z), worldHeight - height_.getMinY()); }

    LevelHeightAccessor height_;
    OpaquePredicate isOpaque_;
    BlockGetter blockGetter_;
    SimpleBitStorage data_;
};

} // namespace carbon
