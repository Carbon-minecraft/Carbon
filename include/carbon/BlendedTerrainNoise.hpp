#pragma once

// Scalar implementation of Java Edition 26.3 BlendedNoise and its
// SmearedPerlinNoise stacks. This is the old_blended_noise function itself;
// it does not build the surrounding density graph or choose worldgen seeds.
#include "carbon/XoroshiroWorldgenNoise.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace carbon {

class BlendedTerrainNoise {
    class SmearedPerlin {
        struct Gradient { int x, y, z; };
        inline static constexpr std::array<Gradient, 16> gradients_{{
            {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},{1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
            {0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1},{1,1,0},{0,-1,1},{-1,1,0},{0,-1,-1}
        }};
        std::array<std::uint8_t, 256> permutations_{};
        double offsetX_{}, offsetY_{}, offsetZ_{}, fudgeYScale_{};

        static int addInt(int a, int b) {
            return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
        }
        int permute(int i) const { return permutations_[static_cast<std::uint32_t>(i) & 255U]; }
        static float dot(int hash, float x, float y, float z) {
            const auto& g = gradients_[static_cast<unsigned>(hash) & 15U];
            return g.x * x + g.y * y + g.z * z;
        }
        static float lerp2(float a, float b, float v00, float v10, float v01, float v11) {
            return Mth::lerp(b, Mth::lerp(a, v00, v10), Mth::lerp(a, v01, v11));
        }
        static float lerp3(float a, float b, float c, float v000, float v100, float v010, float v110,
                           float v001, float v101, float v011, float v111) {
            return Mth::lerp(c, lerp2(a,b,v000,v100,v010,v110), lerp2(a,b,v001,v101,v011,v111));
        }
        static float smoothstep(float x) { return x*x*x*(x*(x*6.0F-15.0F)+10.0F); }
        double computeFudgeY(double originalY, double relativeY) const;
        float sampleAndLerp(int x, int y, int z, float rx, float fudgedRy, float rz, float originalRy) const;
    public:
        template<class Random> SmearedPerlin(Random& random, double fudgeYScale) : fudgeYScale_(fudgeYScale) {
            offsetX_ = random.nextDouble() * 256.0;
            offsetY_ = random.nextDouble() * 256.0;
            offsetZ_ = random.nextDouble() * 256.0;
            for (int i = 0; i < 256; ++i) permutations_[i] = static_cast<std::uint8_t>(i);
            for (int i = 0; i < 256; ++i) {
                const int offset = random.nextInt(256 - i);
                std::swap(permutations_[i], permutations_[offset + i]);
            }
        }
        float get(double x, double y, double z) const;
    };

    struct Layer { SmearedPerlin noise; double frequency; float amplitude; };
    using Stack = std::vector<Layer>;
    Stack minLimit_, maxLimit_, main_;
    double xzScale_{}, yScale_{}, xzFactor_{}, yFactor_{};

    static Stack createStack(XoroshiroRandomSource& random, int firstOctave, double smearScaleY, double valueFactor);
    static float getStack(const Stack& stack, double x, double y, double z);

public:
    explicit BlendedTerrainNoise(XoroshiroRandomSource& random,
        double xzScale, double yScale, double xzFactor,
        double yFactor, double smearScaleMultiplier);

    // x/y/z are block coordinates; source sampler scaling is applied here.
    float sample(double x, double y, double z) const;
};

} // namespace carbon
