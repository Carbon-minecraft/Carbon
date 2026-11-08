#include "carbon/Coordinates.hpp"
#include <cstdint>
#include <iostream>

int main() {
    std::int32_t ax, ay, az, bx, by, bz;
    while (std::cin >> ax >> ay >> az >> bx >> by >> bz) {
        const carbon::Vec3i a{ax, ay, az}, b{bx, by, bz};
        const auto off = a.offset(bx, by, bz);
        const auto scaled = a.multiply(bx);
        const auto crossed = a.cross(b);
        const auto bp = carbon::BlockPos::asLong(ax, ay, az);
        const auto sp = carbon::SectionPos::asLong(ax, ay, az);
        const auto rel = carbon::SectionPos::sectionRelativePos({ax, ay, az});
        const auto rp = carbon::SectionPos{carbon::SectionPos::blockToSectionCoord(ax),
            carbon::SectionPos::blockToSectionCoord(ay), carbon::SectionPos::blockToSectionCoord(az)}.relativeToBlockPos(rel);
        const auto cp = carbon::ChunkPos{ax, az};
        std::cout << a.hashCode() << ' ' << a.compareTo(b) << ' '
                  << off.x << ' ' << off.y << ' ' << off.z << ' '
                  << scaled.x << ' ' << scaled.y << ' ' << scaled.z << ' '
                  << crossed.x << ' ' << crossed.y << ' ' << crossed.z << ' '
                  << bp << ' ' << carbon::BlockPos::getX(bp) << ' ' << carbon::BlockPos::getY(bp) << ' ' << carbon::BlockPos::getZ(bp) << ' '
                  << sp << ' ' << carbon::SectionPos::x(sp) << ' ' << carbon::SectionPos::y(sp) << ' ' << carbon::SectionPos::z(sp) << ' '
                  << static_cast<unsigned>(static_cast<std::uint16_t>(rel)) << ' '
                  << rp.x << ' ' << rp.y << ' ' << rp.z << ' '
                  << cp.pack() << ' ' << carbon::ChunkPos::getX(cp.pack()) << ' ' << carbon::ChunkPos::getZ(cp.pack()) << ' ' << cp.hashCode() << ' '
                  << cp.getRegionX() << ' ' << cp.getRegionZ() << ' ' << cp.getRegionLocalX() << ' ' << cp.getRegionLocalZ() << ' '
                  << cp.getBlockX(15) << ' ' << cp.getBlockZ(15) << '\n';
    }
}
