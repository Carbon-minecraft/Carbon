#include "carbon/RandomSources.hpp"
#include "carbon/XoroshiroWorldgenNoise.hpp"
#include <iomanip>
#include <iostream>
#include <string>
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const auto seed = std::stoll(argv[2]);
    if (std::string(argv[1]).ends_with("trace")) {
        std::cout << std::hex << std::setfill('0');
        const std::string kind = argv[1];
        if (kind == "legacytrace") {
            carbon::LegacyRandomSource random(seed);
            for (int i=0;i<128;++i) {
                const auto value = random.nextDouble();
                std::cout << std::setw(16) << std::bit_cast<std::uint64_t>(value) << ' '
                    << std::setw(16) << std::bit_cast<std::uint64_t>(random.nextDouble()) << ' '
                    << std::dec << random.nextInt(1073741825) << std::hex << '\n';
            }
            return 0;
        }
        if (kind == "singletrace") {
            carbon::SingleThreadedRandomSource random(seed);
            for (int i=0;i<128;++i) {
                const auto value = random.nextDouble();
                std::cout << std::setw(16) << std::bit_cast<std::uint64_t>(value) << ' '
                    << std::setw(16) << std::bit_cast<std::uint64_t>(random.nextDouble()) << ' '
                    << std::dec << random.nextInt(1073741825) << std::hex << '\n';
            }
            return 0;
        }
        if (kind == "xorotrace") {
            carbon::XoroshiroRandomSource random(seed);
            for (int i=0;i<128;++i) {
                const auto value = random.nextDouble();
                std::cout << std::setw(16) << std::bit_cast<std::uint64_t>(value) << ' '
                    << std::setw(16) << std::bit_cast<std::uint64_t>(random.nextDouble()) << ' '
                    << std::dec << random.nextInt(1073741825) << std::hex << '\n';
            }
            return 0;
        }
        return 2;
    }
    if (std::string(argv[1]) == "xoroshiro") {
        auto upgraded = carbon::upgradeSeedTo128bit(seed);
        carbon::Xoroshiro128PlusPlus random(upgraded);
        carbon::Xoroshiro128PlusPlus zero(0, 0);
        std::cout << std::bit_cast<std::int64_t>(upgraded.seedLo) << ' '
            << std::bit_cast<std::int64_t>(upgraded.seedHi) << '\n';
        for (int i=0;i<256;++i) std::cout << random.nextLong() << ' ' << zero.nextLong() << '\n';
        return 0;
    }
    carbon::SingleThreadedRandomSource random(seed);
    std::cout << std::setprecision(17);
    for (int i=0;i<256;++i) {
        if (i==128) random.setSeed(seed);
        std::cout << random.nextInt() << ' ' << random.nextInt(1073741825) << ' '
            << random.nextLong() << ' ' << random.nextBoolean() << ' '
            << random.nextFloat() << ' ' << random.nextDouble() << ' ' << random.nextGaussian() << '\n';
    }
    auto child = random.fork();
    std::cout << child.nextLong() << '\n';
    random.consumeCount(19);
    auto factory = random.forkPositional();
    std::cout << factory.at(-30000000, -64, 30000000).nextLong() << '\n';
    std::cout << factory.fromHashOf(u"minecraft:overworld\U0001F600").nextLong() << '\n';
}
