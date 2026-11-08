#include "carbon/BlendedTerrainNoise.hpp"

#include <cmath>
#include <stdexcept>

namespace carbon {

double BlendedTerrainNoise::SmearedPerlin::computeFudgeY(double originalY, double relativeY) const {
    const double fudgeLimit = originalY >= 0.0 && originalY < relativeY ? originalY : relativeY;
    return Mth::floor(fudgeLimit / fudgeYScale_ + 1.0E-7F) * fudgeYScale_;
}

float BlendedTerrainNoise::SmearedPerlin::sampleAndLerp(
    int x, int y, int z, float rx, float ry, float rz, float originalRy) const {
    const int x0 = permute(x), x1 = permute(addInt(x, 1));
    const int xy00 = permute(addInt(x0, y)), xy01 = permute(addInt(addInt(x0, y), 1));
    const int xy10 = permute(addInt(x1, y)), xy11 = permute(addInt(addInt(x1, y), 1));
    const float d000 = dot(permute(addInt(xy00, z)), rx, ry, rz);
    const float d100 = dot(permute(addInt(xy10, z)), rx - 1.0F, ry, rz);
    const float d010 = dot(permute(addInt(xy01, z)), rx, ry - 1.0F, rz);
    const float d110 = dot(permute(addInt(xy11, z)), rx - 1.0F, ry - 1.0F, rz);
    const float d001 = dot(permute(addInt(addInt(xy00, z), 1)), rx, ry, rz - 1.0F);
    const float d101 = dot(permute(addInt(addInt(xy10, z), 1)), rx - 1.0F, ry, rz - 1.0F);
    const float d011 = dot(permute(addInt(addInt(xy01, z), 1)), rx, ry - 1.0F, rz - 1.0F);
    const float d111 = dot(permute(addInt(addInt(xy11, z), 1)), rx - 1.0F, ry - 1.0F, rz - 1.0F);
    return lerp3(smoothstep(rx), smoothstep(originalRy), smoothstep(rz),
                 d000, d100, d010, d110, d001, d101, d011, d111);
}

float BlendedTerrainNoise::SmearedPerlin::get(double _x, double _y, double _z) const {
    const double x = GradientNoise::wrap(_x) + offsetX_;
    const double y = GradientNoise::wrap(_y) + offsetY_;
    const double z = GradientNoise::wrap(_z) + offsetZ_;
    const int floorX = Mth::floor(x), floorY = Mth::floor(y), floorZ = Mth::floor(z);
    const float relativeX = static_cast<float>(x - floorX);
    const double relativeY = y - floorY;
    const float relativeZ = static_cast<float>(z - floorZ);
    const float fudgedRelativeY = static_cast<float>(relativeY - computeFudgeY(_y, relativeY));
    return sampleAndLerp(floorX, floorY, floorZ, relativeX, fudgedRelativeY, relativeZ,
                         static_cast<float>(relativeY));
}

BlendedTerrainNoise::Stack BlendedTerrainNoise::createStack(
    XoroshiroRandomSource& random, int firstOctave, double smearScaleY, double valueFactor) {
    if (firstOctave > 0) throw std::invalid_argument("firstOctave>0");
    const int octaves = -firstOctave + 1;
    double factor = 1.0;
    valueFactor /= std::pow(2.0, octaves) - 1.0;
    Stack stack;
    stack.reserve(static_cast<std::size_t>(octaves));
    for (int i = octaves - 1; i >= 0; --i) {
        stack.emplace_back(Layer{SmearedPerlin(random, smearScaleY * factor), factor,
                                 static_cast<float>(valueFactor)});
        factor /= 2.0;
        valueFactor *= 2.0;
    }
    return stack;
}

float BlendedTerrainNoise::getStack(const Stack& stack, double x, double y, double z) {
    float value = 0.0F;
    for (const auto& layer : stack) {
        value += layer.amplitude * layer.noise.get(x * layer.frequency, y * layer.frequency, z * layer.frequency);
    }
    return value;
}

BlendedTerrainNoise::BlendedTerrainNoise(XoroshiroRandomSource& random,
    double xzScale, double yScale, double xzFactor, double yFactor, double smearScaleMultiplier)
    : xzScale_(xzScale), yScale_(yScale), xzFactor_(xzFactor), yFactor_(yFactor) {
    const double limitSmearScaleY = (684.412 * yScale) * smearScaleMultiplier;
    const double mainSmearScaleY = limitSmearScaleY / yFactor;
    minLimit_ = createStack(random, -15, limitSmearScaleY, 0.99998474F);
    maxLimit_ = createStack(random, -15, limitSmearScaleY, 0.99998474F);
    main_ = createStack(random, -7, mainSmearScaleY, 12.75);
}

float BlendedTerrainNoise::sample(double x, double y, double z) const {
    const double xzMultiplier = 684.412 * xzScale_;
    const double yMultiplier = 684.412 * yScale_;
    const double xz = x * xzMultiplier;
    const double yz = y * yMultiplier;
    const double zz = z * xzMultiplier;
    const float minLimit = getStack(minLimit_, xz, yz, zz);
    const float maxLimit = getStack(maxLimit_, xz, yz, zz);
    const double mainXzMultiplier = xzMultiplier / xzFactor_;
    const double mainYMultiplier = yMultiplier / yFactor_;
    const float main = getStack(main_, x * mainXzMultiplier, y * mainYMultiplier, z * mainXzMultiplier);
    const float choice = Mth::clamp(main + 0.5F, 0.0F, 1.0F);
    if (choice == 0.0F) return minLimit;
    if (choice == 1.0F) return maxLimit;
    return Mth::lerp(choice, minLimit, maxLimit);
}

} // namespace carbon
