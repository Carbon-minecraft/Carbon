#pragma once

// Binary NBT subset from Java Edition 26.3. Uses the DataInput/DataOutput
// modified UTF-8 format, with in-memory buffers provided by BinaryIO.hpp.
#include "carbon/BinaryIO.hpp"
#include "carbon/Nbt.hpp"

#include <span>

namespace carbon::nbt {

struct NbtIOOptions {
    std::uint64_t maxBytes = 2u * 1024u * 1024u; // NbtAccounter.defaultQuota()
    std::uint32_t maxDepth = 512;
};

// `writeUnnamedTag` writes ID, empty root name, and payload (unless End).
std::vector<std::uint8_t> writeUnnamedTag(const Tag& tag);
std::vector<std::uint8_t> write(const CompoundTag& root);

// `readAnyTag` consumes an ID and payload, with no root name. `readUnnamedTag`
// consumes ID, the root's modified-UTF name, and payload. `read` additionally
// enforces Java NbtIo's named-compound root requirement.
Tag readAnyTag(std::span<const std::uint8_t> bytes, NbtIOOptions options = {});
Tag readUnnamedTag(std::span<const std::uint8_t> bytes, NbtIOOptions options = {});
CompoundTag read(std::span<const std::uint8_t> bytes, NbtIOOptions options = {});

} // namespace carbon::nbt
