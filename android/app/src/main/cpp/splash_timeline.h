#pragma once

#include <array>
#include <cstdint>

namespace nevergone::splash_timeline {

constexpr double kTimelineCompleteSeconds = 5.5;
constexpr double kStartupSoundSeconds = 0.2;

struct Sample {
    float black_overlay_alpha = 0.0f;
    std::array<float, 6> frame_alpha{};
    bool complete = false;
};

Sample sample(double seconds);
Sample sample_tick(std::uint64_t tick);

}  // namespace nevergone::splash_timeline
