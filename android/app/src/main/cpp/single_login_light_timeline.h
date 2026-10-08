#pragma once

#include <array>
#include <cstdint>

namespace nevergone::single_login_light_timeline {

constexpr int kLightCount = 4;

struct Sample {
    std::array<float, kLightCount> alpha{};
};

// Reset to the same PRNG family used by the original client. The original
// AppParameters::init() seeded srand48(time(NULL)); callers supply the seed so
// the timeline remains testable and independent from libc-global PRNG state.
void reset(std::uint64_t seed_seconds);

// scene_seconds is measured from the recovered SingleLogin/createUI boundary.
Sample sample(double scene_seconds);

}  // namespace nevergone::single_login_light_timeline
