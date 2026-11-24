#include "rt64_math.h"

#include <cmath>
#include <cstdio>

int main() {
    // The train and bike both use 16.16 view-projection matrices. One step
    // either side of an axis can put the X depth coefficient below one bit.
    constexpr float depth = 200.0f;
    const hlslpp::float4x4 projection(
        1.5f, 0, 0, 0,
        0, 2, 0, 0,
        0, 0, -(depth + 1) / (depth - 1), -1,
        0, 0, -2 * depth / (depth - 1), 0);
    for (float yaw : {0.0f, 0.0001f, -0.0001f, 1.5708f, 3.14159f, 0.7f}) {
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        const hlslpp::float4x4 view(
            c, 0, -s, 0,
            0, 1, 0, 0,
            s, 0, c, 0,
            0, 0, -50, 1);
        auto guest = hlslpp::mul(view, projection);
        for (unsigned row = 0; row < 4; ++row) {
            for (unsigned col = 0; col < 4; ++col) {
                guest[row][col] = std::trunc(guest[row][col] * 65536.0f) / 65536.0f;
            }
        }

        hlslpp::float4x4 recoveredView, recoveredProjection;
        RT64::matrixDecomposeViewProj(guest, recoveredView, recoveredProjection);
        const float farPlane = RT64::farPlaneFromProj(recoveredProjection);
        if (!(std::abs(farPlane - depth) < 1.0f)) {
            std::fprintf(stderr, "yaw %.6f moved far plane from %.1f to %.3f\n", yaw, depth, farPlane);
            return 1;
        }
    }
    return 0;
}
