#pragma once

// Java Edition 26.3 NormalNoise configuration, normalization, and sampling.
// Construction currently supports the existing LegacyRandomSource positional
// factory; the Xoroshiro positional-factory path is not part of Carbon yet.
#include "carbon/PerlinNoise.hpp"
#include "carbon/LegacyRandomSource.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace carbon {

enum class NormalNoiseNormalization { Disabled, Enabled, Legacy };

class NormalNoise {
public:
    class Builder;

    struct OctaveInfo {
        int octaveIndex;
        double frequency;
        double amplitude;
        std::string seed() const { return "octave_" + std::to_string(octaveIndex); }
        double absAmplitude() const { return std::abs(amplitude); }
    };

private:
    double baseAmplitude_;
    int baseOctave_;
    int octaveCount_;
    NormalNoiseNormalization normalization_;
    std::vector<double> amplitudeModifiers_;
    std::vector<OctaveInfo> octaves_;
    double normalizationFactor_;
    NoiseInterval range_;

    static double getAmplitudeModifier(const std::vector<double>& modifiers, int index) {
        return modifiers.empty() ? 1.0 : modifiers[static_cast<std::size_t>(index)];
    }

    static std::vector<OctaveInfo> buildOctaves(int baseOctave, double baseAmplitude,
            int octaveCount, bool normalize, const std::vector<double>& modifiers) {
        if (octaveCount < 1 || octaveCount > 32)
            throw std::invalid_argument("octave count must be in [1,32]");
        if (!modifiers.empty() && modifiers.size() != static_cast<std::size_t>(octaveCount))
            throw std::invalid_argument("amplitude modifier count must match octave count");
        double frequency = std::pow(2.0, baseOctave);
        double amplitude = baseAmplitude;
        if (normalize) {
            amplitude *= std::pow(0.5, -(octaveCount - 1)) / (std::pow(0.5, -octaveCount) - 1.0);
        }
        std::vector<OctaveInfo> result;
        result.reserve(static_cast<std::size_t>(octaveCount));
        for (int i = 0; i < octaveCount; ++i) {
            const double modifier = getAmplitudeModifier(modifiers, i);
            if (modifier != 0.0)
                result.push_back({baseOctave + i, frequency, amplitude * modifier});
            frequency *= 2.0;
            amplitude *= 0.5;
        }
        return result;
    }

    static double estimateDeviation(const std::vector<OctaveInfo>& octaves) {
        double variance = 0.0;
        for (const auto& octave : octaves) {
            const double layerDeviation = PerlinNoise::STANDARD_DEVIATION * octave.absAmplitude();
            variance += layerDeviation * layerDeviation;
        }
        return std::sqrt(variance);
    }

    static double computeNormalizationFactor(double targetAmplitude, const std::vector<OctaveInfo>& octaves) {
        const double inputDeviation = estimateDeviation(octaves);
        if (inputDeviation == 0.0) return 0.0;
        const double inputSumDeviation = inputDeviation * std::sqrt(2.0);
        const double targetDeviation = targetAmplitude * 0.3333333333333333;
        return targetDeviation / inputSumDeviation;
    }

    static double parityExpectedDeviation(int octaveSpan) {
        return 0.1 * (1.0 + 1.0 / (octaveSpan + 1));
    }

    static double computeParityNormalizationFactor(double baseAmplitude, int octaveCount,
            const std::vector<double>& modifiers) {
        int minOctave = 2147483647;
        int maxOctave = -2147483647 - 1;
        for (int i = 0; i < octaveCount; ++i) {
            if (getAmplitudeModifier(modifiers, i) != 0.0) {
                minOctave = std::min(minOctave, i);
                maxOctave = std::max(maxOctave, i);
            }
        }
        return baseAmplitude * 0.5 * 0.3333333333333333 / parityExpectedDeviation(maxOctave - minOctave);
    }

    NormalNoise(double baseAmplitude, int baseOctave, int octaveCount,
            NormalNoiseNormalization normalization, std::vector<double> modifiers)
        : baseAmplitude_(baseAmplitude), baseOctave_(baseOctave), octaveCount_(octaveCount),
          normalization_(normalization), amplitudeModifiers_(std::move(modifiers)),
          octaves_(buildOctaves(baseOctave_, baseAmplitude_, octaveCount_,
              normalization_ != NormalNoiseNormalization::Disabled, amplitudeModifiers_)),
          normalizationFactor_(0.0), range_{} {
        double targetAmplitude = 0.0;
        for (const auto& octave : octaves_) targetAmplitude += octave.absAmplitude();
        normalizationFactor_ = computeNormalizationFactor(targetAmplitude, octaves_);
        if (normalization_ == NormalNoiseNormalization::Legacy && normalizationFactor_ != 0.0) {
            const double parityFactor = computeParityNormalizationFactor(baseAmplitude_, octaveCount_, amplitudeModifiers_);
            targetAmplitude *= parityFactor / normalizationFactor_;
            normalizationFactor_ = parityFactor;
        }
        range_ = {-static_cast<float>(targetAmplitude * 0.3333333333333333 * 6.0),
                   static_cast<float>(targetAmplitude * 0.3333333333333333 * 6.0)};
    }

public:
    static Builder builder();

    NoiseStack create(LegacyRandomSource& random) const {
        const LegacyPositionalRandomFactory firstRandom(random.nextLong());
        const LegacyPositionalRandomFactory secondRandom(random.nextLong());
        auto stackBuilder = NoiseStack::builder();
        for (const auto& octave : octaves_) {
            const std::string seedName = octave.seed();
            const std::u16string name(seedName.begin(), seedName.end());
            auto first = firstRandom.fromHashOf(name);
            auto second = secondRandom.fromHashOf(name);
            PerlinNoise firstNoise(first);
            PerlinNoise secondNoise(second);
            const float valueFactor = static_cast<float>(normalizationFactor_ * octave.amplitude);
            stackBuilder.add(firstNoise, octave.frequency, valueFactor);
            stackBuilder.add(secondNoise, octave.frequency * 1.0181268882175227, valueFactor);
        }
        return stackBuilder.build();
    }

    NoiseInterval range() const { return range_; }
    double normalizationFactor() const { return normalizationFactor_; }
    const std::vector<OctaveInfo>& octaves() const { return octaves_; }
    int octaveCount() const { return octaveCount_; }
    int baseOctave() const { return baseOctave_; }
    double baseAmplitude() const { return baseAmplitude_; }
    const std::vector<double>& amplitudeModifiers() const { return amplitudeModifiers_; }

    static NormalNoise createParity(int firstOctave, const std::vector<double>& amplitudes) {
        if (amplitudes.empty()) throw std::invalid_argument("Need at least 1 amplitude");
        const auto octaves = buildOctaves(firstOctave, 1.0, static_cast<int>(amplitudes.size()), true, amplitudes);
        double targetAmplitude = 0.0;
        for (const auto& octave : octaves) targetAmplitude += octave.absAmplitude();
        const double newFactor = computeNormalizationFactor(targetAmplitude, octaves);
        double baseAmplitude = 1.0;
        if (newFactor != 0.0) {
            const double oldFactor = computeParityNormalizationFactor(1.0, static_cast<int>(amplitudes.size()), amplitudes);
            baseAmplitude = oldFactor / newFactor;
        }
        return NormalNoise(baseAmplitude, firstOctave, static_cast<int>(amplitudes.size()),
                           NormalNoiseNormalization::Enabled, amplitudes);
    }

    static NormalNoise createParity(int firstOctave, std::initializer_list<double> amplitudes) {
        return createParity(firstOctave, std::vector<double>(amplitudes));
    }
};

class NormalNoise::Builder {
    double baseAmplitude_ = 1.0;
    int baseOctave_ = 0;
    int octaveCount_ = 1;
    NormalNoiseNormalization normalization_ = NormalNoiseNormalization::Enabled;
    std::vector<double> amplitudeModifiers_;
public:
    Builder& setBaseAmplitude(double value) { baseAmplitude_ = value; return *this; }
    Builder& setBaseOctave(int value) { baseOctave_ = value; return *this; }
    Builder& setOctaveCount(int value) {
        if (!amplitudeModifiers_.empty()) throw std::invalid_argument("Cannot set octave count after setting amplitude modifier");
        octaveCount_ = value; return *this;
    }
    Builder& setNormalize(bool value) {
        normalization_ = value ? NormalNoiseNormalization::Enabled : NormalNoiseNormalization::Disabled;
        return *this;
    }
    Builder& setLegacyNormalization() { normalization_ = NormalNoiseNormalization::Legacy; return *this; }
    Builder& setNormalization(NormalNoiseNormalization value) { normalization_ = value; return *this; }
    Builder& setAmplitudeModifier(int index, double value) {
        if (index < 0 || index >= octaveCount_) throw std::invalid_argument("amplitude modifier index outside octave range");
        if (amplitudeModifiers_.empty()) amplitudeModifiers_.assign(static_cast<std::size_t>(octaveCount_), 1.0);
        amplitudeModifiers_[static_cast<std::size_t>(index)] = value;
        return *this;
    }
    NormalNoise build() const {
        return NormalNoise(baseAmplitude_, baseOctave_, octaveCount_, normalization_, amplitudeModifiers_);
    }
};

inline NormalNoise::Builder NormalNoise::builder() { return {}; }

} // namespace carbon
