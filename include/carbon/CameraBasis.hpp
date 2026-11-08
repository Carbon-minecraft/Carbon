#pragma once

#include "carbon/Vec3.hpp"

#include <cmath>
#include <array>

namespace carbon {

struct CameraBasis {
    Vec3 right;
    Vec3 up;
    Vec3 forward;
};

inline CameraBasis cameraBasis(float yawDegrees, float pitchDegrees) {
    constexpr double radians = 3.14159265358979323846 / 180.0;
    const double yaw = yawDegrees * radians;
    const double pitch = pitchDegrees * radians;
    const double sinYaw = std::sin(yaw), cosYaw = std::cos(yaw);
    const double sinPitch = std::sin(pitch), cosPitch = std::cos(pitch);
    return {
        {-cosYaw, 0.0, -sinYaw},
        {-sinYaw * sinPitch, cosPitch, cosYaw * sinPitch},
        {-sinYaw * cosPitch, -sinPitch, cosYaw * cosPitch}
    };
}

inline std::array<double, 16> cameraViewMatrix(float yawDegrees, float pitchDegrees, const Vec3& eye) {
    const CameraBasis camera = cameraBasis(yawDegrees, pitchDegrees);
    const double rx=camera.right.x, ry=camera.right.y, rz=camera.right.z;
    const double fx=camera.forward.x, fy=camera.forward.y, fz=camera.forward.z;
    const double ux=camera.up.x, uy=camera.up.y, uz=camera.up.z;
    return {
        rx, ux, -fx, 0,
        ry, uy, -fy, 0,
        rz, uz, -fz, 0,
        -(rx*eye.x+ry*eye.y+rz*eye.z),
        -(ux*eye.x+uy*eye.y+uz*eye.z),
        fx*eye.x+fy*eye.y+fz*eye.z, 1
    };
}

} // namespace carbon
