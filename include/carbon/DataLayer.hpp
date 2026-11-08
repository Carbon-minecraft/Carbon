#pragma once

// Source-faithful subset of net.minecraft.world.level.chunk.DataLayer.

#include <cstdint>
#include <string>
#include <vector>

namespace carbon {

class DataLayer {
public:
    static constexpr int LAYER_COUNT = 16;
    static constexpr int LAYER_SIZE = 128;
    static constexpr int SIZE = 2048;

    explicit DataLayer(std::int32_t defaultValue = 0);
    explicit DataLayer(std::vector<std::uint8_t> data);

    std::int32_t get(std::int32_t x, std::int32_t y, std::int32_t z) const;
    void set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t value);
    void fill(std::int32_t value);

    // Like Java getData(), this materializes a uniform layer and returns the
    // backing byte array, so callers may mutate it directly.
    std::vector<std::uint8_t>& getData();
    const std::vector<std::uint8_t>* dataIfMaterialized() const noexcept;
    DataLayer copy() const;

    bool isDefinitelyHomogenous() const noexcept;
    bool isDefinitelyFilledWith(std::int32_t value) const noexcept;
    bool isEmpty() const noexcept;
    std::int32_t defaultValue() const noexcept { return defaultValue_; }

    std::string toString() const;
    std::string layerToString(std::int32_t layer) const;

private:
    static std::int32_t index(std::int32_t x, std::int32_t y, std::int32_t z) noexcept;
    std::int32_t getAt(std::int32_t index) const;
    void setAt(std::int32_t index, std::int32_t value);

    std::vector<std::uint8_t> data_;
    std::int32_t defaultValue_ = 0;
    bool materialized_ = false;
};

}
