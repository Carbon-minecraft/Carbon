#include "carbon/NbtIO.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace carbon::nbt;

int main(int argc, char** argv) {
    // Java DataOutput/NbtIo byte fixture: root compound with one named int.
    CompoundTag root; root.putInt(u"x", 42);
    const std::vector<std::uint8_t> expected{10,0,0,3,0,1,'x',0,0,0,42,0};
    assert(write(root) == expected);
    assert(read(expected) == root);

    // 26.3 heterogeneous list representation: element type 10, values encoded
    // as one-key compounds under the empty string, then unwrapped on load.
    ListTag mixed; mixed.add(Tag::intTag(7)); mixed.add(Tag::string(u"s"));
    CompoundTag mixedRoot; mixedRoot.put(u"m", Tag(mixed));
    const std::vector<std::uint8_t> mixedBytes{
        10,0,0,9,0,1,'m',10,0,0,0,2,
        3,0,0,0,0,0,7,0,
        8,0,0,0,1,'s',0,0
    };
    auto actualMixed = write(mixedRoot);
    assert(actualMixed == mixedBytes);
    assert(read(mixedBytes) == mixedRoot);

    // Exercise all payload IDs and UTF-16/modified UTF including NUL and a
    // supplementary character represented by a surrogate pair.
    CompoundTag all;
    all.put(u"b", Tag::byteTag(-3)); all.put(u"h", Tag::shortTag(-1234));
    all.put(u"i", Tag::intTag(0x12345678)); all.put(u"l", Tag::longTag(-2));
    all.put(u"f", Tag::floatTag(1.25f)); all.put(u"d", Tag::doubleTag(-0.0));
    all.put(u"ba", Tag(ByteArray{-1,0,1}));
    all.put(u"s", Tag::string(std::u16string{u'A',0,u'\u00e9',0xd83d,0xde00}));
    ListTag ints; ints.add(Tag::intTag(1)); ints.add(Tag::intTag(2)); all.put(u"li", Tag(ints));
    CompoundTag nested; nested.put(u"v", Tag::string(u"ok")); all.put(u"c", Tag(nested));
    all.put(u"ia", Tag(IntArray{-1,0,1})); all.put(u"la", Tag(LongArray{-1,0,1}));
    assert(read(write(all)) == all);

    auto rejects = [](std::vector<std::uint8_t> bytes, NbtIOOptions opt = {}) {
        try { (void)read(bytes, opt); } catch (const carbon::BinaryIOError&) { return true; }
        return false;
    };
    assert(rejects({13})); // invalid ID
    assert(rejects({10,0,0,3,0,1,'x'})); // truncated int payload
    assert(rejects({10,0,0,9,0,1,'x',0,0,0,0xff,0xff,0xff,0xff})); // negative list length
    assert(rejects({10,0,0,7,0,1,'x',0xff,0xff,0xff,0xff})); // negative array length
    assert(rejects({1,0,0,0}, {})); // named compound root required
    assert(rejects(expected, NbtIOOptions{1,512})); // quota
    assert(rejects(write(mixedRoot), NbtIOOptions{1ull<<30,1})); // nested depth

    if (argc > 1) {
        std::ifstream f(argv[1], std::ios::binary);
        assert(f.good());
        std::vector<std::uint8_t> fixture(std::istreambuf_iterator<char>(f), {});
        const auto parsed = read(fixture, NbtIOOptions{100ull*1024*1024,512});
        const auto reparsed = read(write(parsed), NbtIOOptions{100ull*1024*1024,512});
        assert(parsed == reparsed);
        std::cout << "fixture bytes=" << fixture.size() << " root entries=" << parsed.size() << '\n';
    }
}
