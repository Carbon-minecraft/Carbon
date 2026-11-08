#pragma once

// In-memory primitives corresponding to java.io.DataInput/DataOutput and the
// byte/string subset of net.minecraft.network.FriendlyByteBuf (Java Edition 26.3).
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace carbon {

class BinaryIOError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

namespace binary_io_detail {
inline std::u16string toUtf16(std::string_view s) {
    std::u16string out;
    for (std::size_t i = 0; i < s.size();) {
        const auto b0 = static_cast<std::uint8_t>(s[i++]);
        std::uint32_t cp{};
        if (b0 < 0x80) cp = b0;
        else if ((b0 & 0xe0) == 0xc0 && i < s.size()) {
            const auto b1 = static_cast<std::uint8_t>(s[i++]);
            if ((b1 & 0xc0) != 0x80) throw BinaryIOError("invalid UTF-8");
            cp = ((b0 & 0x1f) << 6) | (b1 & 0x3f);
            if (cp < 0x80) throw BinaryIOError("invalid UTF-8");
        } else if ((b0 & 0xf0) == 0xe0 && i + 1 < s.size()) {
            const auto b1 = static_cast<std::uint8_t>(s[i++]), b2 = static_cast<std::uint8_t>(s[i++]);
            if ((b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80) throw BinaryIOError("invalid UTF-8");
            cp = ((b0 & 15) << 12) | ((b1 & 63) << 6) | (b2 & 63);
            if (cp < 0x800 || (cp >= 0xd800 && cp <= 0xdfff)) throw BinaryIOError("invalid UTF-8");
        } else if ((b0 & 0xf8) == 0xf0 && i + 2 < s.size()) {
            const auto b1 = static_cast<std::uint8_t>(s[i++]), b2 = static_cast<std::uint8_t>(s[i++]), b3 = static_cast<std::uint8_t>(s[i++]);
            if ((b1 & 0xc0) != 0x80 || (b2 & 0xc0) != 0x80 || (b3 & 0xc0) != 0x80) throw BinaryIOError("invalid UTF-8");
            cp = ((b0 & 7) << 18) | ((b1 & 63) << 12) | ((b2 & 63) << 6) | (b3 & 63);
            if (cp < 0x10000 || cp > 0x10ffff) throw BinaryIOError("invalid UTF-8");
        } else throw BinaryIOError("invalid UTF-8");
        if (cp <= 0xffff) out.push_back(static_cast<char16_t>(cp));
        else { cp -= 0x10000; out.push_back(static_cast<char16_t>(0xd800 + (cp >> 10))); out.push_back(static_cast<char16_t>(0xdc00 + (cp & 1023))); }
    }
    return out;
}
inline std::string fromUtf16(std::u16string_view s) {
    std::string out;
    auto emit = [&out](std::uint32_t cp) {
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) { out.push_back(static_cast<char>(0xc0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 63))); }
        else if (cp < 0x10000) { out.push_back(static_cast<char>(0xe0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63))); out.push_back(static_cast<char>(0x80 | (cp & 63))); }
        else { out.push_back(static_cast<char>(0xf0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 63))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63))); out.push_back(static_cast<char>(0x80 | (cp & 63))); }
    };
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::uint32_t c = s[i];
        if (c >= 0xd800 && c <= 0xdbff && i + 1 < s.size() && s[i + 1] >= 0xdc00 && s[i + 1] <= 0xdfff) {
            c = 0x10000 + ((c - 0xd800) << 10) + (s[++i] - 0xdc00);
        } else if (c >= 0xd800 && c <= 0xdfff) c = 0xfffd;
        emit(c);
    }
    return out;
}
}

class ByteWriter {
public:
    void writeByte(std::int8_t v) { bytes_.push_back(static_cast<std::uint8_t>(v)); }
    void writeBoolean(bool v) { writeByte(v ? 1 : 0); }
    void writeShort(std::int16_t v) { writeUnsigned(std::bit_cast<std::uint16_t>(v), 2); }
    void writeInt(std::int32_t v) { writeUnsigned(std::bit_cast<std::uint32_t>(v), 4); }
    void writeLong(std::int64_t v) { writeUnsigned(std::bit_cast<std::uint64_t>(v), 8); }
    // DataOutputStream uses floatToIntBits/doubleToLongBits, canonicalizing NaNs.
    void writeFloat(float v) { writeInt(std::isnan(v) ? std::bit_cast<std::int32_t>(0x7fc00000U) : std::bit_cast<std::int32_t>(v)); }
    void writeDouble(double v) { writeLong(std::isnan(v) ? std::bit_cast<std::int64_t>(0x7ff8000000000000ULL) : std::bit_cast<std::int64_t>(v)); }
    void writeVarInt(std::int32_t v) {
        auto bits = std::bit_cast<std::uint32_t>(v);
        while ((bits & ~std::uint32_t{0x7f}) != 0) { writeByte(static_cast<std::int8_t>((bits & 0x7f) | 0x80)); bits >>= 7; }
        writeByte(static_cast<std::int8_t>(bits));
    }
    void writeVarLong(std::int64_t v) {
        auto bits = std::bit_cast<std::uint64_t>(v);
        while ((bits & ~std::uint64_t{0x7f}) != 0) { writeByte(static_cast<std::int8_t>((bits & 0x7f) | 0x80)); bits >>= 7; }
        writeByte(static_cast<std::int8_t>(bits));
    }
    // DataOutput.writeUTF: modified UTF-8 over Java UTF-16 code units, with an unsigned BE byte count.
    void writeModifiedUtf(std::u16string_view s) {
        std::vector<std::uint8_t> encoded;
        for (char16_t c : s) {
            const auto u = static_cast<std::uint16_t>(c);
            if (u >= 1 && u <= 0x7f) encoded.push_back(static_cast<std::uint8_t>(u));
            else if (u <= 0x7ff) { encoded.push_back(static_cast<std::uint8_t>(0xc0 | (u >> 6))); encoded.push_back(static_cast<std::uint8_t>(0x80 | (u & 63))); }
            else { encoded.push_back(static_cast<std::uint8_t>(0xe0 | (u >> 12))); encoded.push_back(static_cast<std::uint8_t>(0x80 | ((u >> 6) & 63))); encoded.push_back(static_cast<std::uint8_t>(0x80 | (u & 63))); }
        }
        if (encoded.size() > 65535) throw BinaryIOError("modified UTF-8 string exceeds 65535 bytes");
        writeShort(static_cast<std::int16_t>(encoded.size()));
        bytes_.insert(bytes_.end(), encoded.begin(), encoded.end());
    }
    // FriendlyByteBuf UTF-8 string: VarInt byte length, then UTF-8. maxLength counts UTF-16 units.
    void writeUtf(std::string_view s, std::size_t maxLength = 32767) {
        auto utf16 = binary_io_detail::toUtf16(s);
        if (utf16.size() > maxLength) throw BinaryIOError("UTF-8 string exceeds maximum character length");
        if (s.size() > static_cast<std::size_t>(maxLength) * 3) throw BinaryIOError("UTF-8 string exceeds maximum encoded length");
        writeVarInt(static_cast<std::int32_t>(s.size()));
        bytes_.insert(bytes_.end(), s.begin(), s.end());
    }
    const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }
    std::vector<std::uint8_t> takeBytes() && { return std::move(bytes_); }
private:
    template<class U> void writeUnsigned(U v, unsigned count) { for (unsigned i = count; i != 0; --i) bytes_.push_back(static_cast<std::uint8_t>(v >> ((i - 1) * 8))); }
    std::vector<std::uint8_t> bytes_;
};

class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    explicit ByteReader(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
    std::size_t position() const noexcept { return position_; }
    std::size_t remaining() const noexcept { return bytes_.size() - position_; }
    std::int8_t readByte() { return std::bit_cast<std::int8_t>(readUnsigned<std::uint8_t>(1)); }
    bool readBoolean() { return readByte() != 0; }
    std::int16_t readShort() { return std::bit_cast<std::int16_t>(readUnsigned<std::uint16_t>(2)); }
    std::int32_t readInt() { return std::bit_cast<std::int32_t>(readUnsigned<std::uint32_t>(4)); }
    std::int64_t readLong() { return std::bit_cast<std::int64_t>(readUnsigned<std::uint64_t>(8)); }
    float readFloat() { return std::bit_cast<float>(readInt()); }
    double readDouble() { return std::bit_cast<double>(readLong()); }
    std::int32_t readVarInt() {
        std::uint32_t out = 0; unsigned n = 0; std::uint8_t b;
        do { b = static_cast<std::uint8_t>(readByte()); out |= std::uint32_t(b & 127) << ((n * 7) & 31); if (++n > 5) throw BinaryIOError("VarInt too big"); } while ((b & 128) != 0);
        return std::bit_cast<std::int32_t>(out);
    }
    std::int64_t readVarLong() {
        std::uint64_t out = 0; unsigned n = 0; std::uint8_t b;
        do { b = static_cast<std::uint8_t>(readByte()); out |= std::uint64_t(b & 127) << ((n * 7) & 63); if (++n > 10) throw BinaryIOError("VarLong too big"); } while ((b & 128) != 0);
        return std::bit_cast<std::int64_t>(out);
    }
    std::u16string readModifiedUtf() {
        const auto length = static_cast<std::uint16_t>(readShort());
        require(length);
        const auto end = position_ + length;
        std::u16string out;
        while (position_ < end) {
            const auto a = static_cast<std::uint8_t>(readByte());
            if ((a & 0x80) == 0) out.push_back(static_cast<char16_t>(a));
            else if ((a & 0xe0) == 0xc0) {
                if (position_ >= end) throw BinaryIOError("malformed modified UTF-8");
                const auto b = static_cast<std::uint8_t>(readByte());
                if ((b & 0xc0) != 0x80) throw BinaryIOError("malformed modified UTF-8");
                out.push_back(static_cast<char16_t>(((a & 31) << 6) | (b & 63)));
            } else if ((a & 0xf0) == 0xe0) {
                if (end - position_ < 2) throw BinaryIOError("malformed modified UTF-8");
                const auto b = static_cast<std::uint8_t>(readByte()), c = static_cast<std::uint8_t>(readByte());
                if ((b & 0xc0) != 0x80 || (c & 0xc0) != 0x80) throw BinaryIOError("malformed modified UTF-8");
                out.push_back(static_cast<char16_t>(((a & 15) << 12) | ((b & 63) << 6) | (c & 63)));
            } else throw BinaryIOError("malformed modified UTF-8");
        }
        return out;
    }
    std::string readUtf(std::size_t maxLength = 32767) {
        const auto n = readVarInt();
        const auto maxBytes = maxLength > static_cast<std::size_t>(INT32_MAX) / 3 ? static_cast<std::size_t>(INT32_MAX) : maxLength * 3;
        if (n < 0) throw BinaryIOError("negative UTF-8 byte length");
        if (static_cast<std::size_t>(n) > maxBytes) throw BinaryIOError("UTF-8 byte length exceeds maximum");
        require(static_cast<std::size_t>(n));
        std::string s(reinterpret_cast<const char*>(bytes_.data() + position_), static_cast<std::size_t>(n));
        position_ += static_cast<std::size_t>(n);
        if (binary_io_detail::toUtf16(s).size() > maxLength) throw BinaryIOError("UTF-8 string exceeds maximum character length");
        return s;
    }
private:
    void require(std::size_t n) const { if (n > remaining()) throw BinaryIOError("truncated input"); }
    template<class U> U readUnsigned(unsigned count) { require(count); U v = 0; while (count--) v = static_cast<U>((v << 8) | bytes_[position_++]); return v; }
    std::span<const std::uint8_t> bytes_;
    std::size_t position_{};
};

} // namespace carbon
