#pragma once
#include <algorithm>
#include <cmath>

namespace sote::cheats {
struct RapidFireGate {
    bool active = false;
    double started = 0;
    bool fire(bool enabled, float rate, bool held, double now) {
        if (!enabled || !held || !std::isfinite(now)) {
            active = false;
            return held;
        }
        if (!active || now < started) started = now;
        active = true;
        const double period = 1.0 / (std::isfinite(rate) ? std::clamp(rate, 1.0f, 10.0f) : 4.0f);
        return std::fmod(now - started, period) < period * 0.5;
    }
};
}
