#pragma once

// Java Edition 26.3 PerlinNoise and the scalar sampling portion of NoiseStack.
#include "carbon/Math.hpp"
#include "carbon/Noise.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace carbon {

class PerlinNoise {
    struct Gradient { int x, y, z; };
    inline static constexpr std::array<Gradient, 16> GRADIENT{{
        {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
        {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
        {0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1},
        {1,1,0},{0,-1,1},{-1,1,0},{0,-1,-1}
    }};
    std::array<std::uint8_t, 256> perms_{};
    double offsetX_, offsetY_, offsetZ_;
    bool debugEnableFarlands_;

    static int addInt(int a, int b) {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
    }
    int permute(int x) const { return perms_[static_cast<std::uint32_t>(x) & 0xffU]; }
    static float gradDot(int hash, float x, float y, float z) {
        const auto& g = GRADIENT[static_cast<unsigned>(hash) & 15U];
        return g.x * x + g.y * y + g.z * z;
    }
    static float lerp2(float a, float b, float x00, float x10, float x01, float x11) {
        return Mth::lerp(b, Mth::lerp(a, x00, x10), Mth::lerp(a, x01, x11));
    }
    static float lerp3(float a, float b, float c, float x000, float x100, float x010, float x110,
                       float x001, float x101, float x011, float x111) {
        return Mth::lerp(c, lerp2(a,b,x000,x100,x010,x110), lerp2(a,b,x001,x101,x011,x111));
    }
    static float smoothstep(float x) { return x*x*x*(x*(x*6.0F-15.0F)+10.0F); }
    float sampleAndLerp(int x, int y, int z, float rx, float ry, float rz, float originalRy) const {
        const int x0=permute(x), x1=permute(addInt(x,1));
        const int xy00=permute(addInt(x0,y)), xy01=permute(addInt(addInt(x0,y),1));
        const int xy10=permute(addInt(x1,y)), xy11=permute(addInt(addInt(x1,y),1));
        const float d000=gradDot(permute(addInt(xy00,z)),rx,ry,rz);
        const float d100=gradDot(permute(addInt(xy10,z)),rx-1.0F,ry,rz);
        const float d010=gradDot(permute(addInt(xy01,z)),rx,ry-1.0F,rz);
        const float d110=gradDot(permute(addInt(xy11,z)),rx-1.0F,ry-1.0F,rz);
        const float d001=gradDot(permute(addInt(addInt(xy00,z),1)),rx,ry,rz-1.0F);
        const float d101=gradDot(permute(addInt(addInt(xy10,z),1)),rx-1.0F,ry,rz-1.0F);
        const float d011=gradDot(permute(addInt(addInt(xy01,z),1)),rx,ry-1.0F,rz-1.0F);
        const float d111=gradDot(permute(addInt(addInt(xy11,z),1)),rx-1.0F,ry-1.0F,rz-1.0F);
        const float ax=smoothstep(rx), ay=smoothstep(originalRy), az=smoothstep(rz);
        return lerp3(ax,ay,az,d000,d100,d010,d110,d001,d101,d011,d111);
    }
public:
    static constexpr NoiseInterval RANGE{-2.0F, 2.0F};
    static constexpr double STANDARD_DEVIATION=0.2702247831245211;
    template<class R> explicit PerlinNoise(R& random, bool debugEnableFarlands=false)
        : offsetX_(random.nextDouble()*256.0), offsetY_(random.nextDouble()*256.0), offsetZ_(random.nextDouble()*256.0),
          debugEnableFarlands_(debugEnableFarlands) {
        for (int i=0;i<256;++i) perms_[i]=static_cast<std::uint8_t>(i);
        for (int i=0;i<256;++i) {
            const int offset=random.nextInt(256-i);
            std::swap(perms_[i],perms_[offset+i]);
        }
    }
    NoiseInterval range() const { return RANGE; }
    static double wrap(double x, bool debugEnableFarlands=false) { return GradientNoise::wrap(x, debugEnableFarlands); }
    float get(double x, double y) const { return get(wrap(x,debugEnableFarlands_),0.0,wrap(y,debugEnableFarlands_)); }
    float get(double _x, double _y, double _z) const {
        const double x=wrap(_x,debugEnableFarlands_)+offsetX_;
        const double y=wrap(_y,debugEnableFarlands_)+offsetY_;
        const double z=wrap(_z,debugEnableFarlands_)+offsetZ_;
        const int fx=Mth::floor(x), fy=Mth::floor(y), fz=Mth::floor(z);
        const float rx=static_cast<float>(x-fx), ry=static_cast<float>(y-fy), rz=static_cast<float>(z-fz);
        return sampleAndLerp(fx,fy,fz,rx,ry,rz,ry);
    }
};

class NoiseStack {
public:
    struct Layer { PerlinNoise noise; double frequency; float amplitude; };
    class Builder {
        std::vector<Layer> layers_;
    public:
        Builder& add(const PerlinNoise& noise, double frequency, float amplitude) {
            layers_.push_back({noise,frequency,amplitude}); return *this;
        }
        Builder& addStack(const NoiseStack& stack, double frequency, float amplitude) {
            for (const auto& layer : stack.layers_)
                layers_.push_back({layer.noise,layer.frequency*frequency,layer.amplitude*amplitude});
            return *this;
        }
        NoiseStack build() const { return NoiseStack(layers_); }
    };
private:
    std::vector<Layer> layers_;
    explicit NoiseStack(std::vector<Layer> layers) : layers_(std::move(layers)) {}
public:
    static Builder builder() { return {}; }
    NoiseInterval range() const {
        float lo=0.0F, hi=0.0F;
        for (const auto& l:layers_) {
            const float a=l.amplitude*PerlinNoise::RANGE.min, b=l.amplitude*PerlinNoise::RANGE.max;
            lo+=std::min(a,b); hi+=std::max(a,b);
        }
        return {lo,hi};
    }
    float get(double x,double y) const {
        float value=0.0F;
        for (const auto& l:layers_) value += l.amplitude*l.noise.get(x*l.frequency,y*l.frequency);
        return value;
    }
    float get(double x,double y,double z) const {
        float value=0.0F;
        for (const auto& l:layers_) value += l.amplitude*l.noise.get(x*l.frequency,y*l.frequency,z*l.frequency);
        return value;
    }
    std::size_t layerCount() const { return layers_.size(); }
};

}
