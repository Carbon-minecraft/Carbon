#pragma once

// Source-faithful subset of net.minecraft.util.BitStorage,
// SimpleBitStorage, and ZeroBitStorage from the supplied Java 26.3 sources.

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace carbon {

class BitStorage {
public:
    virtual ~BitStorage() = default;
    virtual std::int32_t getAndSet(std::int32_t index, std::int32_t value) = 0;
    virtual void set(std::int32_t index, std::int32_t value) = 0;
    virtual void fill(std::int32_t value) = 0;
    virtual std::int32_t get(std::int32_t index) const = 0;
    virtual std::vector<std::uint64_t>& getRaw() = 0;
    virtual const std::vector<std::uint64_t>& getRaw() const = 0;
    virtual std::int32_t getSize() const = 0;
    virtual std::int32_t getBits() const = 0;
    virtual void getAll(const std::function<void(std::int32_t)>& output) const = 0;
    virtual void unpack(std::vector<std::int32_t>& output) const = 0;
    virtual std::unique_ptr<BitStorage> copy() const = 0;
};

class SimpleBitStorage final : public BitStorage {
public:
    SimpleBitStorage(std::int32_t bits, std::int32_t size);
    SimpleBitStorage(std::int32_t bits, std::int32_t size, const std::vector<std::int32_t>& values);
    SimpleBitStorage(std::int32_t bits, std::int32_t size, std::vector<std::uint64_t> data);

    std::int32_t getAndSet(std::int32_t index, std::int32_t value) override;
    void set(std::int32_t index, std::int32_t value) override;
    void fill(std::int32_t value) override;
    std::int32_t get(std::int32_t index) const override;
    std::vector<std::uint64_t>& getRaw() override { return data_; }
    const std::vector<std::uint64_t>& getRaw() const override { return data_; }
    std::int32_t getSize() const override { return size_; }
    std::int32_t getBits() const override { return bits_; }
    void getAll(const std::function<void(std::int32_t)>& output) const override;
    void unpack(std::vector<std::int32_t>& output) const override;
    std::unique_ptr<BitStorage> copy() const override;

private:
    void checkIndex(std::int32_t index) const;
    void checkValue(std::int32_t value) const;
    std::size_t cellIndex(std::int32_t index) const;
    std::int32_t bits_;
    std::int32_t size_;
    std::int32_t valuesPerLong_;
    std::uint64_t mask_;
    std::vector<std::uint64_t> data_;
};

class ZeroBitStorage final : public BitStorage {
public:
    explicit ZeroBitStorage(std::int32_t size);
    std::int32_t getAndSet(std::int32_t index, std::int32_t value) override;
    void set(std::int32_t index, std::int32_t value) override;
    void fill(std::int32_t value) override;
    std::int32_t get(std::int32_t index) const override;
    std::vector<std::uint64_t>& getRaw() override { return raw_; }
    const std::vector<std::uint64_t>& getRaw() const override { return raw_; }
    std::int32_t getSize() const override { return size_; }
    std::int32_t getBits() const override { return 0; }
    void getAll(const std::function<void(std::int32_t)>& output) const override;
    void unpack(std::vector<std::int32_t>& output) const override;
    std::unique_ptr<BitStorage> copy() const override;
private:
    void checkIndex(std::int32_t index) const;
    static void checkValue(std::int32_t value);
    std::int32_t size_;
    std::vector<std::uint64_t> raw_;
};
}
