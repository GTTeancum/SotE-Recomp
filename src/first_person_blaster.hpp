#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#ifdef SOTE_XBOX
#include "modern_platform.hpp"
#else
#include <mutex>
#endif

namespace sote::first_person_blaster {
struct Pose {
    bool visible = false;
    float sway = 0, lift = 0, recoil = 0;
    bool flash = false;
    uint32_t player = 0;
};

using Matrix = std::array<std::array<float, 4>, 4>;

inline Matrix wrist_in_view(const Pose& pose) {
    // The retail muzzle is 63 mesh units forward of the wrist along -X.
    // Keep the wrist close to the eye and below the lower frustum plane so
    // the receiver enters through the bottom edge, even at the top of a step.
    constexpr float yaw = 0.05f;
    const float pitch = -0.23f - 0.13f * pose.recoil;
    const float sy = std::sin(yaw), cy = std::cos(yaw);
    const float sp = std::sin(pitch), cp = std::cos(pitch);
    return {{{sy * cp, sp, cy * cp, 0}, {sy * sp, -cp, cy * sp, 0}, {cy, 0, -sy, 0},
             {24.0f + 0.9f * pose.sway, -38.5f + 0.6f * pose.lift, -22.0f + 4.5f * pose.recoil, 1}}};
}

// The actor update advances the pose and the foreground draw takes a snapshot.
class Animation {
public:
    void update(uint32_t player, double seconds) {
        Guard lock(mutex);
        if (player != owner) {
            if (tracing()) ::printf("[sote][blaster] visible=%d owner=%08X\n", player != 0, player);
            owner = player;
            shot_age = 1;
            pose = {};
        }
        if (!player) return;
        const float dt = std::isfinite(seconds) ? float(std::clamp(seconds, 0.0, 0.1)) : 0;
        shot_age = std::min(1.f, shot_age + dt);
        pose = {true, 0, 0, std::exp(-16.f * shot_age), shot_age < 0.055f, player};
    }
    void fire(uint32_t shooter) {
        Guard lock(mutex);
        if (!pose.visible || shooter != owner) return;
        shot_age = 0;
        pose.flash = true;
        pose.recoil = 1;
        if (tracing()) ::printf("[sote][blaster] shot owner=%08X\n", shooter);
    }
    Pose current() {
        Guard lock(mutex);
        return pose;
    }
private:
    static bool tracing() {
        static const bool enabled = std::getenv("SOTE_TRACE_FIRST_PERSON_BLASTER") != nullptr;
        return enabled;
    }
#ifdef SOTE_XBOX
    using Guard = XboxInputGuard;
    XboxInputMutex mutex;
#else
    using Guard = std::lock_guard<std::mutex>;
    std::mutex mutex;
#endif
    uint32_t owner = 0;
    float shot_age = 1;
    Pose pose{};
};
inline Animation animation;
}
