#pragma once

// Direct port of Java Edition 26.3 GradientNoise and SimplexNoise.
#include "carbon/Math.hpp"
#include "carbon/RandomSources.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace carbon {

struct NoiseInterval { float min, max; };

class GradientNoise {
protected:
    static int addInt(int a,int b) {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));
    }
    struct Gradient {
        int x, y, z;
        double dot(double a, double b, double c) const { return x * a + y * b + z * c; }
    };
    inline static constexpr std::array<Gradient, 16> GRADIENT{{
        {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
        {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
        {0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1},
        {1,1,0},{0,-1,1},{-1,1,0},{0,-1,-1}
    }};
    std::array<std::uint8_t,256> perms_{};
    double offsetX_, offsetY_, offsetZ_;

    template<class R> explicit GradientNoise(R& random, double scale = 256.0)
      : offsetX_(random.nextDouble() * scale), offsetY_(random.nextDouble() * scale), offsetZ_(random.nextDouble() * scale) {
        for (int i=0;i<256;++i) perms_[i]=static_cast<std::uint8_t>(i);
        for (int i=0;i<256;++i) {
            const int offset=random.nextInt(256-i);
            const auto tmp=perms_[i]; perms_[i]=perms_[offset+i]; perms_[offset+i]=tmp;
        }
    }
    int permute(int x) const { return perms_[static_cast<std::uint32_t>(x)&0xffU]; }
public:
    // Java's DEBUG_ENABLE_FARLANDS is process-wide; the native port makes that
    // flag explicit so callers can supply the same SharedConstants value.
    static double wrap(double x, bool debugEnableFarlands=false) {
        if (debugEnableFarlands) return x;
        constexpr double half=16777215.99999999813735485076904296875;
        return x >= -half && x < half ? x : x - std::floor(x / 3.3554432E7 + 0.5) * 3.3554432E7;
    }
protected:
};

class SimplexNoise final : public GradientNoise {
public:
    static constexpr NoiseInterval RANGE{-2.0F, 2.0F};
    static constexpr double STANDARD_DEVIATION = 0.42544;

    template<class R> explicit SimplexNoise(R& random) : GradientNoise(random) {}
    template<class R> SimplexNoise(R& random, bool discardNoiseOffset)
      : GradientNoise(random, discardNoiseOffset ? 0.0 : 256.0) {}

    NoiseInterval range() const { return RANGE; }

    float get(double _xin, double _yin) const {
        constexpr double SQRT_3=1.73205080756887729352744634150587236;
        constexpr double F2=0.5*(SQRT_3-1.0);
        constexpr double G2=(3.0-SQRT_3)/6.0;
        const double xin=_xin+offsetX_, yin=_yin+offsetY_;
        const double s=(xin+yin)*F2;
        const int i=Mth::floor(xin+s), j=Mth::floor(yin+s);
        const double t=static_cast<double>(addInt(i,j))*G2;
        const double X0=i-t, Y0=j-t;
        const double x0=xin-X0, y0=yin-Y0;
        const int i1=x0>y0?1:0, j1=x0>y0?0:1;
        const double x1=x0-i1+G2, y1=y0-j1+G2;
        const double x2=x0-1.0+2.0*G2, y2=y0-1.0+2.0*G2;
        const int ii=i&0xff, jj=j&0xff;
        const int gi0=permute(ii+permute(jj))%12;
        const int gi1=permute(ii+i1+permute(jj+j1))%12;
        const int gi2=permute(ii+1+permute(jj+1))%12;
        const double n0=corner(gi0,x0,y0,0.0,0.5), n1=corner(gi1,x1,y1,0.0,0.5), n2=corner(gi2,x2,y2,0.0,0.5);
        return static_cast<float>(70.0*(n0+n1+n2));
    }

    float get(double _xin, double _yin, double _zin) const {
        const double xin=_xin+offsetX_, yin=_yin+offsetY_, zin=_zin+offsetZ_;
        const double s=(xin+yin+zin)*0.3333333333333333;
        const int i=Mth::floor(xin+s), j=Mth::floor(yin+s), k=Mth::floor(zin+s);
        const double t=static_cast<double>(addInt(addInt(i,j),k))*0.16666666666666666;
        const double X0=i-t, Y0=j-t, Z0=k-t;
        const double x0=xin-X0, y0=yin-Y0, z0=zin-Z0;
        int i1,j1,k1,i2,j2,k2;
        if(x0>=y0) {
            if(y0>=z0) { i1=1;j1=0;k1=0;i2=1;j2=1;k2=0; }
            else if(x0>=z0) { i1=1;j1=0;k1=0;i2=1;j2=0;k2=1; }
            else { i1=0;j1=0;k1=1;i2=1;j2=0;k2=1; }
        } else if(y0<z0) { i1=0;j1=0;k1=1;i2=0;j2=1;k2=1; }
        else if(x0<z0) { i1=0;j1=1;k1=0;i2=0;j2=1;k2=1; }
        else { i1=0;j1=1;k1=0;i2=1;j2=1;k2=0; }
        const double x1=x0-i1+0.16666666666666666, y1=y0-j1+0.16666666666666666, z1=z0-k1+0.16666666666666666;
        const double x2=x0-i2+0.3333333333333333, y2=y0-j2+0.3333333333333333, z2=z0-k2+0.3333333333333333;
        const double x3=x0-1.0+0.5, y3=y0-1.0+0.5, z3=z0-1.0+0.5;
        const int ii=i&0xff, jj=j&0xff, kk=k&0xff;
        const int gi0=permute(ii+permute(jj+permute(kk)))%12;
        const int gi1=permute(ii+i1+permute(jj+j1+permute(kk+k1)))%12;
        const int gi2=permute(ii+i2+permute(jj+j2+permute(kk+k2)))%12;
        const int gi3=permute(ii+1+permute(jj+1+permute(kk+1)))%12;
        const double n0=corner(gi0,x0,y0,z0,0.6), n1=corner(gi1,x1,y1,z1,0.6);
        const double n2=corner(gi2,x2,y2,z2,0.6), n3=corner(gi3,x3,y3,z3,0.6);
        return static_cast<float>(32.0*(n0+n1+n2+n3));
    }
private:
    static double corner(int index,double x,double y,double z,double base) {
        double t0=base-x*x-y*y-z*z;
        if(t0<0.0) return 0.0;
        t0*=t0;
        return t0*t0*GRADIENT[index].dot(x,y,z);
    }
};
}
