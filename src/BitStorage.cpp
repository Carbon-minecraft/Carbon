#include "carbon/BitStorage.hpp"

#include <algorithm>
#include <bit>
#include <limits>

namespace carbon {
namespace {
std::size_t requiredLength(std::int32_t size, std::int32_t valuesPerLong) {
    // Reproduce Java's signed-int wrap in (size + valuesPerLong - 1), then
    // its truncating signed division. bit_cast avoids C++ signed overflow.
    const auto wrapped = static_cast<std::uint32_t>(size) + static_cast<std::uint32_t>(valuesPerLong) - 1U;
    const auto numerator = std::bit_cast<std::int32_t>(wrapped);
    const std::int64_t length = static_cast<std::int64_t>(numerator) / valuesPerLong;
    if (length < 0 || length > static_cast<std::int64_t>(std::vector<std::uint64_t>().max_size()))
        throw std::length_error("Invalid storage size");
    return static_cast<std::size_t>(length);
}
}

SimpleBitStorage::SimpleBitStorage(std::int32_t bits, std::int32_t size)
    : SimpleBitStorage(bits, size, std::vector<std::uint64_t>([&] {
          if (bits < 1 || bits > 32) throw std::invalid_argument("bits must be between 1 and 32");
          return requiredLength(size, 64 / bits);
      }())) {}

SimpleBitStorage::SimpleBitStorage(std::int32_t bits, std::int32_t size,
                                   const std::vector<std::int32_t>& values)
    : SimpleBitStorage(bits, size) {
    if (values.size() < static_cast<std::size_t>(std::max(size, 0)))
        throw std::out_of_range("Input values shorter than storage size");
    for (std::int32_t i = 0; i < size; ++i) {
        const auto cell = static_cast<std::size_t>(i / valuesPerLong_);
        const auto bit = static_cast<unsigned>((i - static_cast<std::int32_t>(cell) * valuesPerLong_) * bits_);
        const auto value = static_cast<std::uint32_t>(values[static_cast<std::size_t>(i)]) & mask_;
        data_[cell] = (data_[cell] & ~(mask_ << bit)) | (value << bit);
    }
}

SimpleBitStorage::SimpleBitStorage(std::int32_t bits, std::int32_t size,
                                   std::vector<std::uint64_t> data)
    : bits_(bits), size_(size), valuesPerLong_(0), mask_(0), data_(std::move(data)) {
    if (bits < 1 || bits > 32) throw std::invalid_argument("bits must be between 1 and 32");
    valuesPerLong_ = 64 / bits;
    mask_ = (std::uint64_t{1} << bits) - 1;
    if (data_.size() != requiredLength(size, valuesPerLong_))
        throw std::invalid_argument("Invalid length given for storage");
}

void SimpleBitStorage::checkIndex(std::int32_t index) const {
    if (index < 0 || index >= size_) throw std::out_of_range("index outside storage bounds");
}
void SimpleBitStorage::checkValue(std::int32_t value) const {
    if (value < 0 || static_cast<std::uint64_t>(value) > mask_)
        throw std::out_of_range("value outside storage bit range");
}
std::size_t SimpleBitStorage::cellIndex(std::int32_t index) const {
    return static_cast<std::size_t>(index / valuesPerLong_);
}
std::int32_t SimpleBitStorage::get(std::int32_t index) const {
    checkIndex(index);
    const auto cell = cellIndex(index);
    const auto bit = static_cast<unsigned>((index - static_cast<std::int32_t>(cell) * valuesPerLong_) * bits_);
    return static_cast<std::int32_t>((data_[cell] >> bit) & mask_);
}
std::int32_t SimpleBitStorage::getAndSet(std::int32_t index, std::int32_t value) {
    checkIndex(index); checkValue(value);
    const auto old = get(index);
    const auto cell = cellIndex(index);
    const auto bit = static_cast<unsigned>((index - static_cast<std::int32_t>(cell) * valuesPerLong_) * bits_);
    const auto shiftedMask = mask_ << bit;
    data_[cell] = (data_[cell] & ~shiftedMask) | ((static_cast<std::uint64_t>(value) & mask_) << bit);
    return old;
}
void SimpleBitStorage::set(std::int32_t index, std::int32_t value) { (void)getAndSet(index, value); }
void SimpleBitStorage::fill(std::int32_t value) {
    checkValue(value);
    std::uint64_t cell = static_cast<std::uint64_t>(value) & mask_;
    for (unsigned offset = static_cast<unsigned>(bits_); offset < 64; offset <<= 1) cell |= cell << offset;
    std::fill(data_.begin(), data_.end(), cell);
}
void SimpleBitStorage::getAll(const std::function<void(std::int32_t)>& output) const {
    std::int32_t count = 0;
    for (auto cell : data_) {
        for (std::int32_t slot = 0; slot < valuesPerLong_; ++slot) {
            output(static_cast<std::int32_t>(cell & mask_));
            cell >>= bits_;
            if (++count >= size_) return;
        }
    }
}
void SimpleBitStorage::unpack(std::vector<std::int32_t>& output) const {
    if (output.size() < static_cast<std::size_t>(std::max(size_, 0)))
        throw std::out_of_range("output array shorter than storage size");
    std::size_t offset = 0;
    for (std::size_t i = 0; i + 1 < data_.size(); ++i) {
        auto cell = data_[i];
        for (std::int32_t slot = 0; slot < valuesPerLong_; ++slot) {
            output[offset + static_cast<std::size_t>(slot)] = static_cast<std::int32_t>(cell & mask_);
            cell >>= bits_;
        }
        offset += static_cast<std::size_t>(valuesPerLong_);
    }
    const auto remainder = static_cast<std::size_t>(size_) - offset;
    if (remainder > 0) {
        auto cell = data_.back();
        for (std::size_t slot = 0; slot < remainder; ++slot) {
            output[offset + slot] = static_cast<std::int32_t>(cell & mask_);
            cell >>= bits_;
        }
    }
}
std::unique_ptr<BitStorage> SimpleBitStorage::copy() const {
    return std::make_unique<SimpleBitStorage>(bits_, size_, data_);
}

ZeroBitStorage::ZeroBitStorage(std::int32_t size) : size_(size) {}
void ZeroBitStorage::checkIndex(std::int32_t index) const {
    if (index < 0 || index >= size_) throw std::out_of_range("index outside storage bounds");
}
void ZeroBitStorage::checkValue(std::int32_t value) {
    if (value != 0) throw std::out_of_range("value outside zero-bit storage range");
}
std::int32_t ZeroBitStorage::getAndSet(std::int32_t index, std::int32_t value) {
    checkIndex(index); checkValue(value); return 0;
}
void ZeroBitStorage::set(std::int32_t index, std::int32_t value) { (void)getAndSet(index, value); }
void ZeroBitStorage::fill(std::int32_t value) { checkValue(value); }
std::int32_t ZeroBitStorage::get(std::int32_t index) const { checkIndex(index); return 0; }
void ZeroBitStorage::getAll(const std::function<void(std::int32_t)>& output) const {
    for (std::int32_t i = 0; i < size_; ++i) output(0);
}
void ZeroBitStorage::unpack(std::vector<std::int32_t>& output) const {
    if (size_ < 0 || output.size() < static_cast<std::size_t>(size_))
        throw std::out_of_range("invalid output bounds");
    std::fill(output.begin(), output.begin() + size_, 0);
}
std::unique_ptr<BitStorage> ZeroBitStorage::copy() const {
    // Java returns this for ZeroBitStorage.copy(). The C++ API owns polymorphic
    // copies, so return a fresh equivalent object.
    return std::make_unique<ZeroBitStorage>(size_);
}
}
