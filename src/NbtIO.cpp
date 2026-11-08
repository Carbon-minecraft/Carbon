#include "carbon/NbtIO.hpp"

#include <algorithm>
#include <bit>
#include <climits>
#include <string>

namespace carbon::nbt {
namespace {
using Bytes = std::vector<std::uint8_t>;

class Reader {
public:
    Reader(ByteReader& input, NbtIOOptions options) : in(input), quota(options.maxBytes), maxDepth(options.maxDepth) {}
    ByteReader& in;
    std::uint64_t quota, used{};
    std::uint32_t maxDepth, depth{};
    void account(std::uint64_t bytes) {
        if (bytes > quota || used > quota - bytes) throw BinaryIOError("NBT byte quota exceeded");
        used += bytes;
    }
    void push() { if (depth >= maxDepth) throw BinaryIOError("NBT maximum depth exceeded"); ++depth; }
    void pop() { --depth; }
    static std::size_t count(std::int32_t n, std::size_t width = 1) {
        if (n < 0) throw BinaryIOError("negative NBT length");
        const auto size = static_cast<std::size_t>(n);
        if (width && size > std::numeric_limits<std::size_t>::max() / width) throw BinaryIOError("NBT length overflow");
        return size;
    }
};

Tag readPayload(Reader& r, TagId id);
void writePayload(ByteWriter& out, const Tag& tag);

TagId readId(ByteReader& in) {
    const auto raw = static_cast<std::uint8_t>(in.readByte());
    if (raw > 12) throw BinaryIOError("invalid NBT tag ID");
    return static_cast<TagId>(raw);
}

Tag unwrap(Tag t) {
    if (auto* c = t.compound(); c && c->size() == 1) {
        if (const auto* value = c->get(u"")) return value->copy();
    }
    return t;
}

Tag readPayload(Reader& r, TagId id) {
    auto& in = r.in;
    switch (id) {
    case TagId::End: return Tag(EndTag{});
    case TagId::Byte: r.account(9); return Tag::byteTag(in.readByte());
    case TagId::Short: r.account(10); return Tag::shortTag(in.readShort());
    case TagId::Int: r.account(12); return Tag::intTag(in.readInt());
    case TagId::Long: r.account(16); return Tag::longTag(in.readLong());
    case TagId::Float: r.account(12); return Tag::floatTag(in.readFloat());
    case TagId::Double: r.account(16); return Tag::doubleTag(in.readDouble());
    case TagId::ByteArray: {
        r.account(24); const auto n = Reader::count(in.readInt()); r.account(n);
        ByteArray a; a.reserve(n); while (a.size() < n) a.push_back(in.readByte()); return Tag(std::move(a));
    }
    case TagId::String: {
        r.account(36); auto s = in.readModifiedUtf(); r.account(s.size() * 2ull); return Tag(std::move(s));
    }
    case TagId::List: {
        r.push();
        try {
            r.account(36); const auto elementId = readId(in);
            const auto n = Reader::count(in.readInt());
            if (elementId == TagId::End && n != 0) throw BinaryIOError("Missing type on ListTag");
            r.account(static_cast<std::uint64_t>(n) * 4);
            ListTag list;
            for (std::size_t i = 0; i < n; ++i) list.add(unwrap(readPayload(r, elementId)));
            r.pop(); return Tag(list);
        } catch (...) { r.pop(); throw; }
    }
    case TagId::Compound: {
        r.push();
        try {
            r.account(48); CompoundTag c;
            for (;;) {
                auto childId = readId(in);
                if (childId == TagId::End) break;
                auto key = in.readModifiedUtf();
                r.account(28 + key.size() * 2ull);
                auto child = readPayload(r, childId);
                if (!c.contains(key)) r.account(36);
                c.put(std::move(key), std::move(child));
            }
            r.pop(); return Tag(c);
        } catch (...) { r.pop(); throw; }
    }
    case TagId::IntArray: {
        r.account(24); const auto n = Reader::count(in.readInt(), 4); r.account(n * 4ull);
        IntArray a; a.reserve(n); while (a.size() < n) a.push_back(in.readInt()); return Tag(std::move(a));
    }
    case TagId::LongArray: {
        r.account(24); const auto n = Reader::count(in.readInt(), 8); r.account(n * 8ull);
        LongArray a; a.reserve(n); while (a.size() < n) a.push_back(in.readLong()); return Tag(std::move(a));
    }
    }
    throw BinaryIOError("invalid NBT tag ID");
}

Tag wrapped(TagId elementId, const Tag& tag) {
    if (elementId != TagId::Compound) return tag.copy();
    if (const auto* c = tag.compound(); c && !(c->size() == 1 && c->contains(u""))) return tag.copy();
    CompoundTag c; c.put(u"", tag.copy()); return Tag(c);
}

TagId listElementId(const ListTag& list) {
    TagId id = TagId::End;
    for (const auto& value : list.values()) {
        const auto current = value.id();
        // Java's loop uses ID 0 as its "not chosen yet" sentinel. This also
        // means a leading End is replaced by the following element's ID.
        if (id == TagId::End) id = current;
        else if (id != current) return TagId::Compound;
    }
    return id;
}

void writeNamed(ByteWriter& out, std::u16string_view name, const Tag& tag) {
    out.writeByte(static_cast<std::int8_t>(tag.id()));
    if (tag.id() == TagId::End) return;
    out.writeModifiedUtf(name); writePayload(out, tag);
}

void writePayload(ByteWriter& out, const Tag& tag) {
    switch (tag.id()) {
    case TagId::End: return;
    case TagId::Byte: out.writeByte(std::get<std::int8_t>(tag.numeric()->value)); return;
    case TagId::Short: out.writeShort(std::get<std::int16_t>(tag.numeric()->value)); return;
    case TagId::Int: out.writeInt(std::get<std::int32_t>(tag.numeric()->value)); return;
    case TagId::Long: out.writeLong(std::get<std::int64_t>(tag.numeric()->value)); return;
    case TagId::Float: out.writeFloat(std::get<float>(tag.numeric()->value)); return;
    case TagId::Double: out.writeDouble(std::get<double>(tag.numeric()->value)); return;
    case TagId::ByteArray: {
        const auto& a = *tag.byteArray(); if (a.size() > INT32_MAX) throw BinaryIOError("NBT array too large");
        out.writeInt(static_cast<std::int32_t>(a.size())); for (auto b : a) out.writeByte(b); return;
    }
    case TagId::String: out.writeModifiedUtf(*tag.stringValue()); return;
    case TagId::List: {
        const auto& list = *tag.list(); if (list.size() > INT32_MAX) throw BinaryIOError("NBT list too large");
        const auto elementId = listElementId(list); out.writeByte(static_cast<std::int8_t>(elementId)); out.writeInt(static_cast<std::int32_t>(list.size()));
        for (const auto& child : list.values()) writePayload(out, wrapped(elementId, child)); return;
    }
    case TagId::Compound: {
        for (const auto& [key, child] : tag.compound()->entries()) writeNamed(out, key, child);
        out.writeByte(0); return;
    }
    case TagId::IntArray: {
        const auto& a = *tag.intArray(); if (a.size() > INT32_MAX) throw BinaryIOError("NBT array too large");
        out.writeInt(static_cast<std::int32_t>(a.size())); for (auto v : a) out.writeInt(v); return;
    }
    case TagId::LongArray: {
        const auto& a = *tag.longArray(); if (a.size() > INT32_MAX) throw BinaryIOError("NBT array too large");
        out.writeInt(static_cast<std::int32_t>(a.size())); for (auto v : a) out.writeLong(v); return;
    }
    }
}
} // namespace

std::vector<std::uint8_t> writeUnnamedTag(const Tag& tag) {
    ByteWriter out; out.writeByte(static_cast<std::int8_t>(tag.id()));
    if (tag.id() != TagId::End) { out.writeModifiedUtf(u""); writePayload(out, tag); }
    return std::move(out).takeBytes();
}

std::vector<std::uint8_t> write(const CompoundTag& root) { return writeUnnamedTag(Tag(root)); }

Tag readAnyTag(std::span<const std::uint8_t> bytes, NbtIOOptions options) {
    ByteReader in(bytes); const auto id = readId(in); Reader r(in, options);
    if (id == TagId::End) return Tag(EndTag{});
    return readPayload(r, id);
}

Tag readUnnamedTag(std::span<const std::uint8_t> bytes, NbtIOOptions options) {
    ByteReader in(bytes); const auto id = readId(in);
    if (id == TagId::End) return Tag(EndTag{});
    (void)in.readModifiedUtf(); Reader r(in, options); return readPayload(r, id);
}

CompoundTag read(std::span<const std::uint8_t> bytes, NbtIOOptions options) {
    auto tag = readUnnamedTag(bytes, options);
    if (auto* compound = tag.compound()) return compound->copy();
    throw BinaryIOError("Root tag must be a named compound tag");
}

} // namespace carbon::nbt
