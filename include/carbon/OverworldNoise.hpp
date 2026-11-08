#pragma once

// Seeded noise registry for the default Java 26.3 Overworld density assets.
#include "carbon/XoroshiroWorldgenNoise.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace carbon {
class OverworldNoise {
public:
    explicit OverworldNoise(std::int64_t seed);
    float sample(const std::string& identifier, double x, double y, double z);
private:
    XoroshiroPositionalRandomFactory random_;
    std::unordered_map<std::string,std::unique_ptr<XoroshiroNormalNoise>> instances_;
    XoroshiroNormalNoise& get(const std::string& identifier);
};
}
