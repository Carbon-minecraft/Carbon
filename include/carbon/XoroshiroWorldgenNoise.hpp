#pragma once

// Xoroshiro positional construction used by Java 26.3's default (non-legacy)
// worldgen random state. Kept separate from the legacy random API so callers
// cannot accidentally mix the two seed algorithms.
#include "carbon/AdvancedNoise.hpp"
#include "carbon/RandomSources.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace carbon {

class XoroshiroRandomSource {
    Xoroshiro128PlusPlus rng_;
    bool hasGaussian_ = false;
    double gaussian_ = 0.0;
    static std::uint64_t nextBits64(Xoroshiro128PlusPlus& rng, int bits) {
        if (bits < 1 || bits > 64) throw std::invalid_argument("bits must be in [1,64]");
        return static_cast<std::uint64_t>(rng.nextLong()) >> (64 - bits);
    }
public:
    explicit XoroshiroRandomSource(std::int64_t seed) : rng_(upgradeSeedTo128bit(seed)) {}
    explicit XoroshiroRandomSource(Seed128bit seed) : rng_(seed) {}
    XoroshiroRandomSource(std::uint64_t lo, std::uint64_t hi) : rng_(lo, hi) {}
    std::int64_t nextLong() { return rng_.nextLong(); }
    std::int32_t nextInt() { return static_cast<std::int32_t>(rng_.nextLong()); }
    std::int32_t nextInt(std::int32_t bound) {
        if (bound <= 0) throw std::invalid_argument("Bound must be positive");
        std::uint64_t bits = static_cast<std::uint32_t>(nextInt());
        std::uint64_t product = bits * static_cast<std::uint32_t>(bound);
        std::uint32_t low = static_cast<std::uint32_t>(product);
        if (low < static_cast<std::uint32_t>(bound)) {
            const std::uint32_t threshold = (0U - static_cast<std::uint32_t>(bound)) % static_cast<std::uint32_t>(bound);
            while (low < threshold) {
                bits = static_cast<std::uint32_t>(nextInt());
                product = bits * static_cast<std::uint32_t>(bound);
                low = static_cast<std::uint32_t>(product);
            }
        }
        return static_cast<std::int32_t>(product >> 32);
    }
    bool nextBoolean() { return (static_cast<std::uint64_t>(rng_.nextLong()) & 1U) != 0; }
    float nextFloat() { return static_cast<float>(nextBits64(rng_, 24)) * 5.9604645E-8F; }
    double nextDouble() { return static_cast<double>(nextBits64(rng_, 53)) * 1.1102230246251565e-16; }
    double nextGaussian() {
        if (hasGaussian_) { hasGaussian_ = false; return gaussian_; }
        double x, y, r;
        do { x = 2.0 * nextDouble() - 1.0; y = 2.0 * nextDouble() - 1.0; r = x*x + y*y; } while (r >= 1.0 || r == 0.0);
        const double m = std::sqrt(-2.0 * std::log(r) / r);
        gaussian_ = y * m; hasGaussian_ = true; return x * m;
    }
    void consumeCount(int rounds) { for (int i=0; i<rounds; ++i) rng_.nextLong(); }
    XoroshiroRandomSource fork() { return {static_cast<std::uint64_t>(rng_.nextLong()), static_cast<std::uint64_t>(rng_.nextLong())}; }
    Seed128bit forkPositionalSeed() { return {static_cast<std::uint64_t>(rng_.nextLong()), static_cast<std::uint64_t>(rng_.nextLong())}; }
};

namespace detail {
inline constexpr std::array<std::uint32_t,64> MD5_K{{
  0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
  0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
  0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
  0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
  0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
  0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
  0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
  0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 }};
inline constexpr std::array<int,64> MD5_S{{7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
  5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
  6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21}};
inline std::array<std::uint8_t,16> md5(std::string_view input) {
    const std::uint64_t bitLength = static_cast<std::uint64_t>(input.size()) * 8;
    std::size_t size = input.size() + 1;
    while (size % 64 != 56) ++size;
    std::vector<std::uint8_t> bytes(size + 8, 0);
    std::memcpy(bytes.data(), input.data(), input.size()); bytes[input.size()] = 0x80;
    for (int i=0;i<8;++i) bytes[size+i] = static_cast<std::uint8_t>(bitLength >> (8*i));
    std::uint32_t h0=0x67452301,h1=0xefcdab89,h2=0x98badcfe,h3=0x10325476;
    for (std::size_t chunk=0; chunk<bytes.size(); chunk+=64) {
        std::uint32_t m[16];
        for(int i=0;i<16;++i) m[i]=static_cast<std::uint32_t>(bytes[chunk+i*4]) | (static_cast<std::uint32_t>(bytes[chunk+i*4+1])<<8) | (static_cast<std::uint32_t>(bytes[chunk+i*4+2])<<16) | (static_cast<std::uint32_t>(bytes[chunk+i*4+3])<<24);
        std::uint32_t a=h0,b=h1,c=h2,d=h3;
        for(int i=0;i<64;++i) {
            std::uint32_t f; int g;
            if(i<16){f=(b&c)|(~b&d);g=i;} else if(i<32){f=(d&b)|(~d&c);g=(5*i+1)%16;}
            else if(i<48){f=b^c^d;g=(3*i+5)%16;} else {f=c^(b|~d);g=(7*i)%16;}
            const auto next=d; d=c;c=b;b += std::rotl(a+f+MD5_K[i]+m[g],MD5_S[i]);a=next;
        }
        h0+=a;h1+=b;h2+=c;h3+=d;
    }
    std::array<std::uint8_t,16> out{}; std::uint32_t hs[4]={h0,h1,h2,h3};
    for(int w=0;w<4;++w) for(int i=0;i<4;++i) out[w*4+i]=static_cast<std::uint8_t>(hs[w]>>(8*i));
    return out;
}
inline std::uint64_t readBigLong(const std::uint8_t* p) {
    std::uint64_t v=0; for(int i=0;i<8;++i) v=(v<<8)|p[i]; return v;
}
} // namespace detail

class XoroshiroPositionalRandomFactory {
    std::uint64_t lo_, hi_;
public:
    explicit XoroshiroPositionalRandomFactory(Seed128bit seed):lo_(seed.seedLo),hi_(seed.seedHi){}
    XoroshiroRandomSource fromHashOf(std::string_view name) const {
        const auto hash=detail::md5(name);
        return {detail::readBigLong(hash.data()) ^ lo_, detail::readBigLong(hash.data()+8) ^ hi_};
    }
    XoroshiroRandomSource fromSeed(std::int64_t seed) const {
        const auto u=static_cast<std::uint64_t>(seed); return {u^lo_,u^hi_};
    }
    XoroshiroRandomSource at(std::int32_t x,std::int32_t y,std::int32_t z) const {
        const auto positional=static_cast<std::uint64_t>(positionalSeed(x,y,z)); return {positional^lo_,hi_};
    }
};

// 26.3 NormalNoise construction using the default Xoroshiro random source.
class XoroshiroNormalNoise {
    double factor_ = 0;
    NoiseStack stack_ = NoiseStack::builder().build();
public:
    XoroshiroNormalNoise(XoroshiroRandomSource& random, const NormalNoise& parameters) {
        XoroshiroPositionalRandomFactory first(static_cast<Seed128bit>(random.forkPositionalSeed()));
        XoroshiroPositionalRandomFactory second(static_cast<Seed128bit>(random.forkPositionalSeed()));
        auto builder=NoiseStack::builder();
        double target=0, variance=0;
        const auto& octaves=parameters.octaves();
        for(const auto& o:octaves){target+=std::abs(o.amplitude);const double d=PerlinNoise::STANDARD_DEVIATION*std::abs(o.amplitude);variance+=d*d;}
        const double deviation=std::sqrt(variance), nf=deviation==0?0:(target/3.0)/(deviation*std::sqrt(2.0));
        factor_=nf;
        for(const auto& o:octaves){
            const auto name=o.seed(); auto r0=first.fromHashOf(name),r1=second.fromHashOf(name);
            PerlinNoise p0(r0),p1(r1);
            const float a=static_cast<float>(nf*o.amplitude);
            builder.add(p0,o.frequency,a).add(p1,o.frequency*1.0181268882175227,a);
        }
        stack_=builder.build();
    }
    float get(double x,double y,double z) const { return stack_.get(x,y,z); }
    float get(double x,double z) const { return stack_.get(x,z); }
    double normalizationFactor() const { return factor_; }
};

} // namespace carbon
