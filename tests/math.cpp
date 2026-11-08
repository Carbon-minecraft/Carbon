#include "carbon/Math.hpp"
#include "carbon/Vec3.hpp"

#include <bit>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

static double number(const std::string& s) { return std::stod(s); }
static void printFloat(float v) { std::cout << std::bit_cast<std::uint32_t>(v) << '\n'; }
static void printDouble(double v) { std::cout << std::bit_cast<std::uint64_t>(v) << '\n'; }

int main() {
    const carbon::Vec3 p{1.0, 2.0, 3.0}, q{4.0, 6.0, 8.0};
    const auto cross = p.cross(q);
    if (cross.x != -2.0 || cross.y != 4.0 || cross.z != -2.0 || p.dot(q) != 40.0 ||
        p.distanceToSqr(q) != 50.0 || p.add(q).z != 11.0 || p.lerp(q, 0.5).y != 4.0 ||
        carbon::Vec3{1.0e-6, 0.0, 0.0}.normalize().lengthSqr() != 0.0 ||
        !p.normalize().isFinite()) return 3;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string op, a, b, c;
        in >> op >> a;
        const double x = number(a);
        if (op == "floorf") std::cout << carbon::Mth::floor(static_cast<float>(x)) << '\n';
        else if (op == "floord") std::cout << carbon::Mth::floor(x) << '\n';
        else if (op == "lfloor") std::cout << carbon::Mth::lfloor(x) << '\n';
        else if (op == "ceilf") std::cout << carbon::Mth::ceil(static_cast<float>(x)) << '\n';
        else if (op == "ceild") std::cout << carbon::Mth::ceil(x) << '\n';
        else if (op == "ceilLong") std::cout << carbon::Mth::ceilLong(x) << '\n';
        else if (op == "wrapi") std::cout << carbon::Mth::wrapDegrees(static_cast<std::int32_t>(x)) << '\n';
        else if (op == "wrapf") printFloat(carbon::Mth::wrapDegrees(static_cast<float>(x)));
        else if (op == "wrapd") printDouble(carbon::Mth::wrapDegrees(x));
        else if (op == "sin") printFloat(carbon::Mth::sin(x));
        else if (op == "cos") printFloat(carbon::Mth::cos(x));
        else {
            in >> b >> c;
            const double y = number(b), z = number(c);
            if (op == "lerpf") printFloat(carbon::Mth::lerp(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
            else if (op == "lerpd") printDouble(carbon::Mth::lerp(x, y, z));
            else if (op == "clampf") printFloat(carbon::Mth::clamp(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
            else if (op == "clampd") printDouble(carbon::Mth::clamp(x, y, z));
            else return 2;
        }
    }
}
