#pragma once
#include <atomic>
namespace sote::diagnostics {
inline std::atomic<int> input_phase{0}, capture_phase{0}, present_phase{0}, vi_phase{0}, frame_phase{0};
inline std::atomic<unsigned long long> inputs{0}, captures{0}, presents{0};
struct Phase {
    std::atomic<int>& value;
    explicit Phase(std::atomic<int>& v) : value(v) { value.store(1); }
    void set(int v) { value.store(v); }
    ~Phase() { value.store(0); }
};
}
