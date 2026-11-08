#include "carbon/BinaryIO.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <iostream>
#include <iomanip>
#include <vector>

using namespace carbon;

int main() {
    ByteWriter w;
    w.writeByte(-1); w.writeBoolean(true); w.writeShort(-32768);
    w.writeInt(0x12345678); w.writeLong(-2);
    w.writeFloat(1.0f); w.writeDouble(-0.0);
    w.writeVarInt(0); w.writeVarInt(127); w.writeVarInt(128); w.writeVarInt(-1);
    w.writeVarLong(0); w.writeVarLong(128); w.writeVarLong(-1);
    w.writeModifiedUtf(std::u16string{u'A', 0, u'\u00e9', 0xd83d, 0xde00});
    w.writeUtf("A\xc3\xa9\xf0\x9f\x98\x80");
    const std::vector<std::uint8_t> expected = {
        0xff,0x01,0x80,0x00,0x12,0x34,0x56,0x78,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
        0x3f,0x80,0,0, 0x80,0,0,0,0,0,0,0,
        0x00, 0x7f, 0x80,0x01, 0xff,0xff,0xff,0xff,0x0f,
        0x00, 0x80,0x01, 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x01,
        0x00,0x0b, 0x41,0xc0,0x80,0xc3,0xa9,0xed,0xa0,0xbd,0xed,0xb8,0x80,
        0x07, 0x41,0xc3,0xa9,0xf0,0x9f,0x98,0x80
    };
    assert(w.bytes() == expected);

    ByteReader r(w.bytes());
    assert(r.readByte() == -1 && r.readBoolean() && r.readShort() == -32768);
    assert(r.readInt() == 0x12345678 && r.readLong() == -2);
    assert(r.readFloat() == 1.0f && std::bit_cast<std::uint64_t>(r.readDouble()) == 0x8000000000000000ULL);
    assert(r.readVarInt() == 0 && r.readVarInt() == 127 && r.readVarInt() == 128 && r.readVarInt() == -1);
    assert(r.readVarLong() == 0 && r.readVarLong() == 128 && r.readVarLong() == -1);
    assert((r.readModifiedUtf() == std::u16string{u'A', 0, u'\u00e9', 0xd83d, 0xde00}));
    assert(r.readUtf() == "A\xc3\xa9\xf0\x9f\x98\x80" && r.remaining() == 0);

    for (auto b : expected) std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b);
    std::cout << '\n';

    bool truncated = false;
    try { ByteReader shortInput(std::vector<std::uint8_t>{0,0,0}); (void)shortInput.readInt(); }
    catch (const BinaryIOError&) { truncated = true; }
    assert(truncated);
    bool overlong = false;
    try { ByteReader malformed(std::vector<std::uint8_t>{0x80,0x80,0x80,0x80,0x80,0x00}); (void)malformed.readVarInt(); }
    catch (const BinaryIOError&) { overlong = true; }
    assert(overlong);
}
